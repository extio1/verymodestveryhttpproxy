#include "../include/httpproxy.h"
#include "../include/connection.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <pthread.h>

int main(int argc, char** argv)
{
    int port;
    if(argc < 2){
        printf("Please enter the port.\n");
        return -1;
    }

    port = atoi(argv[1]);
    if(port == 0){
        printf("Please enter the port.\n");
        return -1;
    }


    if( start_proxy(port) != 0 ){
        perror("Proxy error");
        return -1;
    }

    connection_t* conn = init_connection();
    connect_to(conn, "vk.com");

    return 0;
}