#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define NAME_SIZE 30
#define BUF_SIZE 1024

typedef struct
{
    SSL * ssl;
    char name[NAME_SIZE];
} clnt_info_pkt;

void setup_socket(int * sock, struct sockaddr_in * serv_addr, const char * ip, int port);
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
    const char * server_ip = argv[1];
    int server_port = atoi(argv[2]);

    // Initilizing OpenSSL
    SSL_library_init();
    SSL_load_error_strings(); // SSL 오류 메시지 가져오기. 
    OpenSSL_add_all_algorithms(); // 알고리즘 가져오기.

    // Initilizing SSL/TLS Context 
    const SSL_METHOD * method = TLS_client_method(); // TLS 연결에 있어서 client 역할을 하겠다.
    SSL_CTX * ctx = SSL_CTX_new(method); // CTX는 Context로, SSL/TLS 연결을 설정하고 관리하기 위한 데이터 구조. SSL/TLS 연결에 필요한 설정과 상태 정보를 저장.
    if(ctx == NULL)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    int sock;
    struct sockaddr_in serv_addr;
    pthread_t send_thread, recv_thread;

    setup_socket(&sock, &serv_addr, server_ip, server_port); // argv[1] : IP, argv[2] : port

    // Create SSL object
    SSL * ssl = SSL_new(ctx);

    // Connect SSL object to socket
    SSL_set_fd(ssl, sock);

    // Establish SSL connection
    SSL_connect(ssl);

    // Build client info packet
    clnt_info_pkt clnt_info;
    clnt_info.ssl = ssl;
    strcpy(clnt_info.name, argv[3]);

    SSL_write(ssl, clnt_info.name, strlen(clnt_info.name));

    char msg[BUF_SIZE];
    int str_len = SSL_read(ssl, msg, BUF_SIZE);
    msg[str_len] = '\0';
    printf("%s", msg);

    const char * direction = "\n< Command >\n"
                             "1. list up the participants\n"
                             "2. exit\n";
    printf("%s", direction);

    pthread_create(&send_thread, NULL, send_msg, (void*)&clnt_info);
    pthread_create(&recv_thread, NULL, recv_msg, (void*)&ssl);
    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sock);
    return 0;
}
void * send_msg(void * arg)
{
    clnt_info_pkt * clnt_info = (clnt_info_pkt*)arg;
    SSL * ssl;
    char name[NAME_SIZE];

    ssl = clnt_info->ssl;
    strcpy(name, clnt_info->name);

    char * command = (char *)malloc(BUF_SIZE);
    while(1)
    {
        printf("[%s] ", name);
        fgets(command, BUF_SIZE, stdin);
        command[strlen(command) - 1] = '\0';

        if(strcmp(command, "1") == 0)
        {
            SSL_write(ssl, "Show the paticipants", strlen("Show the paticipants"));
        }
        else if(strcmp(command, "2") == 0)
        {
            SSL_write(ssl, "exit", strlen("exit"));
            break;
        }
        else
        {
            SSL_write(ssl, command, strlen(command));
        }
    }
    return NULL;
}
void * recv_msg(void * arg)
{
    SSL ** ssl_ptr = (SSL**)arg;
    SSL * ssl = *ssl_ptr;
    char msg[BUF_SIZE];
    int str_len;

    while(1)
    {
        memset(msg, 0, BUF_SIZE);
        str_len = SSL_read(ssl, msg, BUF_SIZE - 1);
        if(str_len <= 0)
        {
            return NULL;
        }
        msg[str_len] = '\0';
        printf("%s", msg);
    }
}

void setup_socket(int * sock, struct sockaddr_in * serv_addr, const char * ip, int port)
{
    *sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(serv_addr, 0, sizeof(*serv_addr));
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_addr.s_addr = inet_addr(ip);
    serv_addr->sin_port = htons(port);

    if(connect(*sock, (struct sockaddr*)serv_addr, sizeof(*serv_addr)) == -1)
        error_handling("connect() error");
}

void error_handling(char * msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}