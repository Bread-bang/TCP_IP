#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 1024

void setup_socket(int * sock, struct sockaddr_in * serv_addr, char * ip, char * port);
void error_handling(char * msg);
void * send_msg(void * arg);
void * recv_msg(void * arg);

int main(int argc, char * argv[])
{
    if(argc != 4) 
    {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    int sock;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;

    setup_socket(&sock, &serv_addr, argv[1], argv[2]); // argv[1] : IP, argv[2] : port

    // pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);
    pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);

    // pthread_join(snd_thread, NULL);
    pthread_join(rcv_thread, NULL);

    close(sock);
    return 0;
}

/*
void * send_msg(void * arg)
{

}
*/

void * recv_msg(void * arg)
{
    int sock = *(int*)arg;
    char msg[BUF_SIZE];

    int str_len;
    while(1)
    {
        str_len = read(sock, msg, BUF_SIZE - 1);
        msg[str_len] = 0;
        printf("%s", msg);
    }
}

void setup_socket(int * sock, struct sockaddr_in * serv_addr, char * ip, char * port)
{
    *sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(serv_addr, 0, sizeof(*serv_addr));
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_addr.s_addr = inet_addr(ip);
    serv_addr->sin_port = htons(atoi(port));

    if(connect(*sock, (struct sockaddr*)serv_addr, sizeof(*serv_addr)) == -1)
        error_handling("connect() error");
}

void error_handling(char * msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}