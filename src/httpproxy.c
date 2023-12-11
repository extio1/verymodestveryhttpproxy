#include "../include/httpproxy.h"
#include "../include/connection.h"
#include "../include/log.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <stdio.h>

static int create_listening_socket(const int port);
static int server_loop(const int sockfd);

int 
start_proxy(const int port)
{
    int listening_socket;

    if( port <= 0 || port >= 65536 ){
        perror("Port incorrect");
        return -1;
    }

    if( (listening_socket = create_listening_socket(port)) == -1){
        perror("create_listening_socket error");
        return -1;
    }

    log(INFO, "created srv listening socket %d", listening_socket); 
    
    if( server_loop(listening_socket) != 0){
        perror("server loop error");
        return -1;
    }

    return 0; // unreachable
}

int 
server_loop(const int srv_sock)
{
    while(1){
        connection_t* conn = init_connection();

        if( (conn->in_sock = accept(srv_sock, &conn->inaddr, &conn->inaddr_len)) == -1){
            perror("socket accept() error");
            return -1;
        }

        conn->in_sock_opened = true;
        log(INFO, "new client accepted on %d", conn->in_sock);

        execute(conn);
    }
}

int 
create_listening_socket(const int port)
{
    int sockfd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    bzero(&addr, addrlen);

    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        perror("socket() error");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if( bind(sockfd, (struct sockaddr*)&addr, addrlen) == -1 ){
        perror("socket bind() error");
        return -1;
    }

    if( listen(sockfd, N_QUEUE_LISTEN_SOCKET) == -1){
        perror("socket listen() error");
        return -1;
    }

    return sockfd;
}