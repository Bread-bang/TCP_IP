#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 1024
#define MAX_CLNT 256

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mtx;

void setup_sock(int * serv_sock, struct sockaddr_in * serv_addr, char * port);
void error_handling(char * msg);
void * handle_clnt(void * arg);

int main(int argc, char * argv[])
{
    if(argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    
    setup_sock(&serv_sock, &serv_addr, argv[1]); // argv[1] is port number

    printf("Server socket setup complete\n");

    pthread_t t_id;
    pthread_mutex_init(&mtx, NULL);

    while(1)
    {
        // printf("Waiting for client connection...\n");
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if(clnt_sock == -1)
        {
            perror("accept() error\n");
            continue;
        }

        // printf("Connected client IP: %s\n", inet_ntoa(clnt_addr.sin_addr));

        pthread_mutex_lock(&mtx);
            clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mtx);

        // printf("Current the number of clients: %d명\n", clnt_cnt);

        pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
        pthread_detach(t_id);
    }

    close(serv_sock);
    return 0;
}

void * handle_clnt(void * arg)
{
    int clnt_sock = *(int *)arg;
    char * msg[BUF_SIZE];

    // Send welcome message to client
    const char * welcome_drink = "\nWelcome to Simple Chat Server!\n"
                                 "------------------------------\n"
                                 "< Command List >\n"
                                 "1. List the current users\n"
                                 "2. Exit\n";
    
    write(clnt_sock, welcome_drink, strlen(welcome_drink));
}

void setup_sock(int * serv_sock, struct sockaddr_in * serv_addr, char * port)
{
    *serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(*serv_sock == -1)
        error_handling("socket() error");

    memset(serv_addr, 0, sizeof(serv_addr));
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr->sin_port = htons(atoi(port));

    // set server socket to reuseable
    int reuseable = 1;
    setsockopt(*serv_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&reuseable, sizeof(reuseable));

    if(bind(*serv_sock, (struct sockaddr*)serv_addr, sizeof(*serv_addr)) == -1)
        error_handling("bind() error");

    if(listen(*serv_sock, 5) == -1)
        error_handling("listen() error");
}

void error_handling(char * msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
