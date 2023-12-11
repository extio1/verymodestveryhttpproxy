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

    conn->parse_context = malloc(sizeof(struct parse_context));
    conn->parse_context->last_header_callback = NONE;

    conn->req_message = init_message();

    conn->req_parser = malloc(sizeof(http_parser));
    conn->resp_parser = malloc(sizeof(http_parser));

    conn->curr_state = req_from_client;

    http_parser_init(conn->req_parser, HTTP_BOTH);
    http_parser_settings_init(&conn->req_setting);
    // conn->req_parser.flags |= HTTP_PARSER_FLAG_SKIPBODY
    conn->req_setting.on_header_field = header_field_cb;
    conn->req_setting.on_header_value = header_value_cb;
    conn->req_setting.on_headers_complete = header_complete_cb;
    conn->req_setting.on_body = body_store_cb;
    conn->req_setting.on_url = on_url_cb;
    conn->req_setting.on_message_complete = req_complete_cb;
    conn->req_parser->data = conn;

    http_parser_init(conn->resp_parser, HTTP_RESPONSE);
    http_parser_settings_init(&conn->resp_setting);
    conn->resp_setting.on_message_complete = resp_complete_cb;
    conn->resp_parser->data = conn;

    conn->keep_alive = false;
    return conn;
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
    mess->body = NULL;
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
static const char* raw_http_version(http_parser* parser);

int 
message_raw(http_parser* parser, char** outbuf, size_t* outsize)
{
    connection_t* conn = (connection_t*)parser->data;
    http_message_t* message = (http_message_t*)conn->req_message;
    
    *outbuf = NULL;
    *outsize = 0;

    const char* method = http_method_str(parser->method);
    add_bytes_to_buffer(outbuf, method, outsize, strlen(method));
    add_bytes_to_buffer(outbuf, " ", outsize, 1);
   
    if(message->request != NULL){
        add_bytes_to_buffer(outbuf, message->request->data, outsize, message->request->len);
    } else if(message->status != NULL){
        add_bytes_to_buffer(outbuf, message->status->data, outsize, message->status->len);
    }
    add_bytes_to_buffer(outbuf, raw_http_version(parser), outsize, 11);

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
raw_http_version(http_parser* parser)
{
    if(parser->http_major > 0 && parser->http_minor > 0){
        return " HTTP/1.1\r\n";
    } else {
        return " HTTP/1.0\r\n";
    }
}
