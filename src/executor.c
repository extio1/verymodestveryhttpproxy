#include "../include/connection.h"
#include "../include/log.h"

#include <http_parser.h>
#include <pthread.h>
#include <unistd.h>

static void* worker_routine(void* arg);

int 
execute(connection_t* connection)
{
    pthread_t tid;

    if( pthread_create(&tid, NULL, worker_routine, connection) != 0 ){
        perror("pthread_create() error");
        return -1;
    }

    if( pthread_detach(tid) != 0){
        perror("pthread_detach() error");
        return -1;
    }

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! // //
    pthread_exit(NULL);                                        //
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! // //

    return 0;
}

void*
worker_routine(void* arg)
{
    connection_t* conn = (connection_t*) arg;

    while(conn->curr_state != NULL){
        conn->curr_state(conn);
    }

    close_connection(conn);
    return NULL;
}

void 
req_from_client(connection_t* conn)
{
    size_t nread, nparsed;
    while(conn->curr_state == req_from_client){
        // Client request reading
        if( (nread = read(conn->in_sock, conn->raw_buffer, conn->raw_bufsize)) == -1 ){
            log(ERROR, "read() error");
            conn->curr_state = NULL;
        }

        nparsed = http_parser_execute(conn->req_parser, &conn->req_setting, conn->raw_buffer, nread);
        
        if(nparsed != nread){
            fprintf(stderr, http_errno_name(HTTP_PARSER_ERRNO(conn->req_parser)));
            fprintf(stderr, http_errno_description(HTTP_PARSER_ERRNO(conn->req_parser)));
            conn->curr_state = NULL;
        }

        // !!!!!!! CLOSING MOST PROBABLY GOING ON A WRONG WAY
        if(nread == 0){
            conn->curr_state = NULL;
        }
    }
}

void 
req_to_srv(connection_t* conn)
{
    size_t nwrite = 0;
    size_t sendall = 0;
    char* raw;
    size_t size;
    message_raw(conn->req_parser, &raw, &size);


    while(sendall < size){
        if( (nwrite = write(conn->dst_sock, &raw[sendall], size-sendall)) == -1 ){
            log(ERROR, "write() error");
            conn->curr_state = NULL;
            return;
        }
        sendall += nwrite;
    }

    conn->curr_state = resp_from_srv;
}


void 
resp_from_srv(connection_t* conn)
{
    size_t nread, nparsed, nwrite = 0;

    while(conn->curr_state == resp_from_srv){
        if( (nread = read(conn->dst_sock, conn->raw_buffer, conn->raw_bufsize)) == -1 ){
            log(ERROR, "read() error");
            conn->curr_state = NULL;
        }
        
        nparsed = http_parser_execute(conn->resp_parser, &conn->resp_setting, conn->raw_buffer, nread);
        
        if(nparsed != nread){
            log(ERROR, http_errno_name(HTTP_PARSER_ERRNO(conn->resp_parser)));
            log(ERROR, http_errno_description(HTTP_PARSER_ERRNO(conn->resp_parser)));
            conn->curr_state = NULL;
        }

        // !!!!!!! CLOSING MOST PROBABLY GOING ON A WRONG WAY
        if(nread == 0){
            conn->curr_state = NULL;
        }     
    }
}

void
resp_to_clt(connection_t* conn)
{
    printf("RESPONDING TO CLT\n");
    sleep(10000);
}
