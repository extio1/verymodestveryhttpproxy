#ifndef OS_PROXY_HTTP_CALLBACK
#define OS_PROXY_HTTP_CALLBACK 1

#include <http_parser.h>

int header_field_cb(http_parser *parser, const char *p, size_t len);
int header_value_cb(http_parser *parser, const char *p, size_t len);
int body_store_cb(http_parser *parser, const char *p, size_t len);
int req_complete_cb(http_parser* parser);
int header_complete_cb(http_parser* parser);
int on_url_cb(http_parser* parser, const char *at, size_t length);
int on_chunk_header(http_parser* parser);
int on_chunk_complete(http_parser* parser);

int resp_complete_cb(http_parser* parser);
int on_status_cb(http_parser* parser, const char *at, size_t length);

#endif /* OS_PROXY_HTTP_CALLBACK */
