#include "../include/httpcallback.h"
#include "../include/connection.h"
#include "../include/log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void alloc_new_field_buffer(struct parse_context*, const char*, size_t);
static int realloc_new_field_buffer(struct parse_context*, const char*, size_t);
static void alloc_new_value_buffer(struct parse_context*, const char*, size_t);
static int realloc_new_value_buffer(struct parse_context*, const char*, size_t);

int
resp_complete_cb(http_parser* parser)
{
    connection_t* conn = parser->data;

    conn->curr_state = resp_to_clt;
    return 0;
}

int 
req_complete_cb(http_parser* parser)
{
    connection_t* conn = parser->data;
    http_message_t* currmess = conn->req_message;
    list_t* header = currmess->header;

    header_key_t host_domain = list_get(header, "Host");
    if( host_domain != NULL){
        connect_to(conn, host_domain);
    } else {
        log(ERROR, "connection impossible, no Host field in http message");
        return -1;
    }
    
    conn->curr_state = req_to_srv;
    return 0;
}

int 
header_complete_cb(http_parser* parser)
{
    connection_t* conn = (connection_t*) parser->data;
    struct parse_context *parse_context = conn->parse_context;
    list_append(conn->req_message->header,      
                parse_context->field_buffer, parse_context->value_buffer
    );

    return 0;
}

int 
on_url_cb(http_parser* parser, const char *at, size_t length)
{
    connection_t* conn = (connection_t*)parser->data;
    http_message_t *currmess = (http_message_t *)conn->req_message;

    if(currmess->request == NULL){
        currmess->request = array_init(length);
    }

    array_extend(currmess->request, at, length);
    
    return 0;
}

int 
body_store_cb(http_parser *parser, const char *p, size_t len)
{
    connection_t* conn = (connection_t*) parser->data;
    http_message_t* currmess = conn->req_message;

    if(currmess->body == NULL){
        currmess->body = array_init(len);
    }

    if( array_extend(currmess->body, p, len) != 0 ){
        log(ERROR, "array extending error");
        return -1;
    }

    return 0;
}

int
header_field_cb(http_parser *parser, const char *p, size_t len)
{
    connection_t* conn = (connection_t*) parser->data;
    struct parse_context *parse_context = conn->parse_context;

    /*
     * Constructing header field string.
     * Depending on the last callback allocate, reallocate buffers
     * Store it to field-value list if new one is starting to proccesed
     * https://github.com/nodejs/http-parser/tree/main#:~:text=fits%20your%20application.-,Reading%20headers,-may%20be%20a
     */

    if(parse_context->last_header_callback == NONE){ // first call
        alloc_new_field_buffer(parse_context, p, len);
    } else if (parse_context->last_header_callback == FIELD){ // previous name continues
        if( realloc_new_field_buffer(parse_context, p, len) != 0) {
            return -1;
        }
    } else if (parse_context->last_header_callback == VALUE){ // new header started
        // append into header list
        list_append(conn->req_message->header,      
                    parse_context->field_buffer, parse_context->value_buffer
        );
        //start new buffer
        alloc_new_field_buffer(parse_context, p, len);
    } 

    parse_context->last_header_callback = FIELD;
    return 0;
}

int
header_value_cb(http_parser *parser, const char *p, size_t len)
{
    connection_t* conn = (connection_t*) parser->data;
    struct parse_context *parse_context = conn->parse_context;
    
    if(parse_context->last_header_callback == FIELD){ // value for current field started
        alloc_new_value_buffer(parse_context, p, len);
    } else if (parse_context->last_header_callback == VALUE){ // previous value continues
        if( realloc_new_value_buffer(parse_context, p, len) != 0){
            return -1;
        }
    }

    parse_context->last_header_callback = VALUE;

    return 0;
}

void 
alloc_new_field_buffer(struct parse_context* parse_context, const char* p, size_t len)
{
    parse_context->field_buffer = malloc(len+1);
    parse_context->field_str_start_indx = len - 1;
    strncpy(parse_context->field_buffer, p, len);
    parse_context->field_buffer[len] = '\0';
}

int
realloc_new_field_buffer(struct parse_context* parse_context, const char* p, size_t len)
{
    char* prev = parse_context->field_buffer;
    char* new_buff = realloc(prev, strlen(prev)+len+1); // adding null-terminator
    if(new_buff == NULL){
        log(ERROR, "realloc buffer error");
        return -1;
    }
    parse_context->field_buffer = new_buff;
    strncpy(&parse_context->field_buffer[parse_context->field_str_start_indx], p, len);
    parse_context->field_str_start_indx += len - 1;
    parse_context->field_buffer[parse_context->field_str_start_indx+1] = '\0';
    return 0;
}

void 
alloc_new_value_buffer(struct parse_context* parse_context, const char* p, size_t len)
{
    parse_context->value_buffer = malloc(len+1);
    parse_context->value_str_start_indx = len - 1;
    strncpy(parse_context->value_buffer, p, len);
    parse_context->value_buffer[len] = '\0';
}

int
realloc_new_value_buffer(struct parse_context* parse_context, const char* p, size_t len)
{
    char* prev = parse_context->value_buffer;
    char* new_buff = realloc(prev, strlen(prev)+len+1);
    if(new_buff == NULL){
        log(ERROR, "realloc buffer error");
        return -1;
    }
    parse_context->value_buffer = new_buff;
    strncpy(&parse_context->value_buffer[parse_context->value_str_start_indx], p, len);
    parse_context->value_str_start_indx += len - 1;
    parse_context->value_buffer[parse_context->value_str_start_indx+1] = '\0'; 
    return 0;
}