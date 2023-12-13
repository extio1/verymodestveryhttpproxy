#include "../include/log.h"
#include "../include/connection.h"
#include "../include/configure.h"
#include "../include/httpcallback.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h> 
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>

#define PORT_STR_MAX_LEN 5

connection_t* 
init_connection()
{
    connection_t* conn = calloc(1, sizeof(connection_t));

    conn->in_sock_opened = false;
    conn->dst_sock_opened = false;
    conn->inaddr_len = sizeof(conn->inaddr);
    conn->raw_bufsize = WORKER_BUFFER_SIZE;
    conn->raw_buffer = malloc(conn->raw_bufsize*sizeof(char));

    conn->current_mode = REQUEST;
    conn->req_message = init_message();
    conn->resp_message = init_message();

    conn->req_parser = malloc(sizeof(http_parser));
    conn->resp_parser = malloc(sizeof(http_parser));

    conn->curr_state = req_from_client;

    http_parser_init(conn->req_parser, HTTP_BOTH);
    http_parser_settings_init(&conn->req_setting);
    // http general parse callbacks
    conn->req_setting.on_header_field = header_field_cb;
    conn->req_setting.on_header_value = header_value_cb;
    conn->req_setting.on_headers_complete = header_complete_cb;
    conn->req_setting.on_body = body_store_cb;
    // conn->req_setting.on_chunk_complete = on_chunk_complete;
    // conn->req_setting.on_chunk_header = on_chunk_header;
    // specific
    conn->req_setting.on_url = on_url_cb;
    conn->req_setting.on_message_complete = req_complete_cb;
    conn->req_parser->data = conn;
    conn->resp_setting.on_message_complete = resp_complete_cb;

    http_parser_init(conn->resp_parser, HTTP_RESPONSE);
    http_parser_settings_init(&conn->resp_setting);
    // http general parse callbacks
    conn->resp_setting.on_header_field = header_field_cb;
    conn->resp_setting.on_header_value = header_value_cb;
    conn->resp_setting.on_headers_complete = header_complete_cb;
    conn->resp_setting.on_body = body_store_cb;
    conn->resp_setting.on_chunk_complete = on_chunk_complete;
    conn->resp_setting.on_chunk_header = on_chunk_header;
    // specific
    conn->resp_setting.on_message_complete = resp_complete_cb;
    conn->resp_setting.on_status = on_status_cb;
    conn->resp_parser->data = conn;
    conn->resp_parser->flags |= F_SKIPBODY;

    conn->keep_alive = false;
    return conn;
}

http_message_t* 
get_current_message(connection_t* conn)
{
    switch (conn->current_mode)
    {
    case REQUEST:
        return conn->req_message;
    case RESPONSE:
        return conn->resp_message;
    default:
        return NULL;
    }
}

void 
close_connection(connection_t* conn)
{
    if(conn->in_sock_opened)
        close(conn->in_sock);

    if(conn->dst_sock_opened)
        close(conn->dst_sock);

    free(conn->resp_parser);
    free(conn->req_parser);
    free(conn->raw_buffer);
    free(conn);
}

http_message_t* 
init_message()
{
    http_message_t* mess = malloc(sizeof(http_message_t));

    mess->header = list_init();
    mess->body = array_init(0);

    mess->parse_context = malloc(sizeof(struct parse_context));
    mess->parse_context->last_header_callback = NONE;
    
    mess->request = NULL;
    mess->status = NULL;

    return mess;
}

int 
connect_to(connection_t* conn, const char* dom)
{
    struct addrinfo hints, *res, *p;
    char* domain = malloc(strlen(dom));
    char ipstr[INET6_ADDRSTRLEN]; 
    char portstr[PORT_STR_MAX_LEN] = "";
    int status;

    strcpy(domain, dom);
    for(int i = 0; i < strlen(domain); ++i){
        if(domain[i] == ':'){
            strcpy(portstr, &domain[i+1]);
            domain[i] = '\0';
            break;
        }
    }

    if(strcmp(portstr, "") == 0){
        memcpy(portstr, "80\0", 3);
    }

    log(INFO, "resolving %s:%s", domain, portstr);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    if ((status = getaddrinfo(domain, portstr, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    for(p = res; p != NULL; p = p->ai_next) {
        void* addr;
        conn->dst_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(conn->dst_sock == -1){
            log(ERROR, "creating socket error");
            continue;
        }

        if (p->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        } else {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        if( connect(conn->dst_sock, p->ai_addr, p->ai_addrlen) != 0 ){
            log(ERROR, "connect error");
            close(conn->dst_sock);
            continue;
        } else {
            inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
            log(INFO, "connected to %s:%s", ipstr, portstr);
            break;
        }
    }

    freeaddrinfo(res);
    free(domain);
    return 0; 
}

#define BUFFER_START_SIZE 100
static int add_to_buffer_headers(list_t* list, char** outbuf, size_t* outsize);
static int add_to_buffer_field(char** to, char* from, size_t* outsize, size_t len);
static int add_to_buffer_value(char** to, char* from, size_t* outsize, size_t len);
static int add_bytes_to_buffer(char **buffer, const char* src, size_t *buf_size, size_t src_size);
static const char* raw_http_version(const unsigned short http_major, const unsigned short http_minor);

int 
response_raw(http_parser* parser, char** outbuf, size_t* outsize)
{
    connection_t* conn = (connection_t*)parser->data;
    http_message_t* message = (http_message_t*)conn->resp_message;
    
    char status_code[10];
    sprintf(status_code, "%d", parser->status_code);
    const char* http_ver = raw_http_version(parser->http_minor, parser->http_major);

    *outbuf = NULL;
    *outsize = 0;

    add_bytes_to_buffer(outbuf, http_ver, outsize, strlen(http_ver));
    add_bytes_to_buffer(outbuf, " ", outsize, 1);

    add_bytes_to_buffer(outbuf, status_code, outsize, strlen(status_code));
    add_bytes_to_buffer(outbuf, " ", outsize, 1);

    add_bytes_to_buffer(outbuf, message->status->data, outsize, message->status->len);
    add_bytes_to_buffer(outbuf, "\r\n", outsize, 2);

    add_to_buffer_headers(message->header, outbuf, outsize);
    
    add_bytes_to_buffer(outbuf, "\r\n", outsize, 2);
    
    if(message->body != NULL)
        add_bytes_to_buffer(outbuf, message->body->data, outsize, message->body->len);

    return 0;
}

int 
request_raw(http_parser* parser, char** outbuf, size_t* outsize)
{
    connection_t* conn = (connection_t*)parser->data;
    http_message_t* message = (http_message_t*)conn->req_message;
    
    const char* http_ver = raw_http_version(parser->http_minor, parser->http_major);
    const char* method = http_method_str(parser->method);

    *outbuf = NULL;
    *outsize = 0;

    add_bytes_to_buffer(outbuf, method, outsize, strlen(method));
    add_bytes_to_buffer(outbuf, " ", outsize, 1);
   
    add_bytes_to_buffer(outbuf, message->request->data, outsize, message->request->len);
    
    add_bytes_to_buffer(outbuf, " ", outsize, 1);
    add_bytes_to_buffer(outbuf, http_ver, outsize, strlen(http_ver));
    add_bytes_to_buffer(outbuf, "\r\n", outsize, 2);

    add_to_buffer_headers(message->header, outbuf, outsize);
    
    add_bytes_to_buffer(outbuf, "\r\n", outsize, 2);
    
    if(message->body != NULL)
        add_bytes_to_buffer(outbuf, message->body->data, outsize, message->body->len);


    return 0;
}

int 
add_bytes_to_buffer(char **buffer, const char* src, size_t *buf_size, size_t src_size)
{
    char *nb = realloc(*buffer, *buf_size+src_size);
    if(nb == NULL){
        log(ERROR, "realloc error");
        return -1;
    }
    *buffer = nb;

    memcpy(&(*buffer)[*buf_size], src, src_size);
    *buf_size += src_size;
    
    return 0;
}

int
add_to_buffer_headers(list_t* list, char** outbuf, size_t* outsize)
{
    node_t* node = list->first_entry;

    if(node == NULL)
        return 1;

    add_to_buffer_field(outbuf, node->key, outsize, strlen(node->key));
    add_to_buffer_value(outbuf, node->value, outsize, strlen(node->value));

    node_t* nnode = node->next_entry;

    while(nnode != NULL) {
        add_to_buffer_field(outbuf, nnode->key, outsize, strlen(nnode->key));
        add_to_buffer_value(outbuf, nnode->value, outsize, strlen(nnode->value));
        nnode = nnode->next_entry;
    }

    return -1;
}

int 
add_to_buffer_field(char** to, char* from,
                    size_t* outsize, size_t len)
{

    add_bytes_to_buffer(to, from, outsize, len);
    add_bytes_to_buffer(to, ": ", outsize, 2);

    return 0;
}

int
add_to_buffer_value(char** to, char* from,
                    size_t* outsize, size_t len)
{    
    add_bytes_to_buffer(to, from, outsize, len);
    add_bytes_to_buffer(to, "\r\n", outsize, 2);

    return 0;
}

const char* 
raw_http_version(const unsigned short http_major, const unsigned short http_minor)
{
    if(http_major > 0 && http_minor > 0){
        return "HTTP/1.1";
    } else {
        return "HTTP/1.0";
    }
}
