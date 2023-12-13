#ifndef OS_PROXY_CONNECTION_H
#define OS_PROXY_CONNECTION_H

#include "map.h"
#include "array.h"

#include <http_parser.h>
#include <netinet/in.h>
#include <stdbool.h>

typedef struct connection connection_t;

typedef void (*worker_state)(connection_t* connection);

struct parse_context
{
    // header is (FIELD=VALUE)*
    enum last_header_callback {NONE=0, FIELD, VALUE} last_header_callback;

    char* field_buffer;
    size_t field_str_start_indx;

    char* value_buffer;
    size_t value_str_start_indx;
};

typedef struct httpmessage
{
    array_t* request;
    array_t* status;
    list_t* header;
    array_t* body;

    struct parse_context* parse_context;
} http_message_t;

http_message_t* init_message();

struct connection {
    bool keep_alive;

    bool in_sock_opened;
    bool dst_sock_opened;
    int in_sock;
    int dst_sock;

    struct sockaddr inaddr;
    socklen_t inaddr_len;

    uint16_t raw_bufsize;
    char* raw_buffer;

    http_parser_settings req_setting;
    http_parser_settings resp_setting;
    http_parser *req_parser;
    http_parser *resp_parser;

    worker_state curr_state;

    /* PRIVATE */
    enum current_mode { REQUEST=0, RESPONSE } current_mode;
    http_message_t* req_message;   // getting current message request or response
    http_message_t* resp_message;  // handles via get_current_message(connection_t*)
};

int execute(connection_t* connection);

void req_from_client(connection_t* connection);
void req_to_srv(connection_t* connection);
void resp_from_srv(connection_t* connection);
// int resp_to_clt_from_cache(connection_t* connection);
void resp_to_clt(connection_t* connection);

connection_t* init_connection();
void close_connection(connection_t* conn);

// domain is "domain_name:port" is there's only "domain_name" port'll be 80
int connect_to(connection_t* conn, const char* domain);

int request_raw(http_parser* parser, char** outbuf, size_t* outsize);
int response_raw(http_parser* parser, char** outbuf, size_t* outsize);

http_message_t* get_current_message(connection_t* conn);

#endif /* OS_PROXY_CONNECTION_H */