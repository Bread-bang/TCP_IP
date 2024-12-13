#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <openssl/ssl.h> // 일반적으로 TLS을 구현할 때 ssl.h을 사용. tls1.h는 TLS 프로토콜의 세부 사항이 필요할 때만 직접 포함한다.
#include <openssl/err.h>

#define BUF_SIZE 1024
#define MAX_CLNT 256
#define NAME_SIZE 100 

typedef struct 
{
    SSL * ssl;
    int clnt_sock;
    int client_idx;
} clnt_info_pkt;

int clnt_cnt = 0;
pthread_mutex_t mtx;

void setup_sock(int * serv_sock, struct sockaddr_in * serv_addr, char * port);
void error_handling(char * msg);
void * handle_clnt(void * arg);
void SSL_error_handling(char * err_msg, SSL * ssl, clnt_info_pkt * clnt_info);

SSL_CTX * create_context(); // SSL_CTX 객체를 TLS_server_method 함수를 사용하여 생성.

int main(int argc, char * argv[])
{
    if(argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // Initializing OpenSSL
    SSL_library_init();
    SSL_load_error_strings(); // SSL 오류 메시지 가져오기. 
    OpenSSL_add_all_algorithms(); // 알고리즘 가져오기.

    // Initilizing SSL/TLS Context 
    SSL_CTX * ctx = create_context();
    
    // Load server certificate and private key
    if(SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0)
    {
        perror("Unable to use certificate file");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if(SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0)
    {
        perror("Unable to use private key file");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    
    setup_sock(&serv_sock, &serv_addr, argv[1]); // argv[1] is port number

    pthread_t send_thread;
    pthread_t recv_thread;
    pthread_mutex_init(&mtx, NULL);

    while(1)
    {
        SSL * ssl;
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if(clnt_sock == -1)
        {
            perror("accept() error\n");
            continue;
        }

        ssl = SSL_new(ctx); // 새로운 SSL 연결을 생성하고 CTX에 연결을 설정.
        SSL_set_fd(ssl, clnt_sock); // SSL 연결에 클라이언트 소켓을 할당.

        int str_len = SSL_accept(ssl);
        if(str_len <= 0) // TLS Handshake 수행. 0을 반환하면 성공적으로 연결된 것.
        {
            ERR_print_errors_fp(stderr); // ERR_print_errors_fp 함수는 OpenSSL의 모든 오류를 편리하기 출력해주는 함수.
            printf("SSL_accept() failed\n");
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(clnt_sock);
            continue;
        }

        clnt_info_pkt * clnt_info = (clnt_info_pkt *)malloc(sizeof(clnt_info_pkt));

        // Limit the number of clients
        pthread_mutex_lock(&mtx);
            int current_cnt = clnt_cnt;
            if(current_cnt < MAX_CLNT)
            {
                clnt_info->ssl = ssl;
                clnt_info->clnt_sock = clnt_sock;
                clnt_info->client_idx = clnt_cnt;
                clnt_cnt++;
            }
            else
            {
                SSL_shutdown(ssl);
                SSL_free(ssl);
                close(clnt_sock);
                continue;
            }
        pthread_mutex_unlock(&mtx);
        printf("Current client count: %d\n", clnt_cnt);

        pthread_create(&recv_thread, NULL, handle_clnt, (void*)clnt_info);
        pthread_detach(recv_thread);
    }

    close(serv_sock);
    return 0;
}
void * handle_clnt(void * arg)
{
    SSL * ssl;
    int read_len;
    char name[NAME_SIZE];
    char msg[BUF_SIZE];
    clnt_info_pkt * clnt_info = (clnt_info_pkt *)arg;

    ssl = clnt_info->ssl;
    memset(name, 0, NAME_SIZE);

    // Read client's name first
    read_len = SSL_read(ssl, name, NAME_SIZE - 1);
    if(read_len <= 0)
    {
        SSL_error_handling("SSL_read() error", ssl, clnt_info);
        pthread_mutex_lock(&mtx);
        clnt_cnt--;
        pthread_mutex_unlock(&mtx);
        return NULL;
    }
    name[read_len] = '\0';

    time_t now;
    time(&now);
    struct tm * time_info = localtime(&now);

    // printf("-----------------------------------\n");
    printf("[%02d:%02d:%02d] \"%s\" joined \n", time_info->tm_hour, time_info->tm_min, time_info->tm_sec, name);
    // printf("-----------------------------------\n");

    // Send welcome message to client
    const char * welcome_drink =  "-----------------------------------\n"
                                  "  Welcome to Simple Chat Server!\n"
                                  "-----------------------------------\n";
    int write_len = SSL_write(ssl, welcome_drink, strlen(welcome_drink));
    if (write_len <= 0) 
    {
        SSL_error_handling("Failed to send welcome message to \"%s\"\n", ssl, clnt_info);
        pthread_mutex_lock(&mtx);
        clnt_cnt--;
        pthread_mutex_unlock(&mtx);
        return NULL;
    } 
    else 
    {
        printf("\t  - Served the welcome drink to \"%s\"\n\n", name);
    }

   

    // Message handling
    while(1)
    {
        time(&now);
        time_info = localtime(&now); 

        read_len = SSL_read(ssl, msg, BUF_SIZE - 1);

        if(read_len <= 0)
        {
            break;
        }
        msg[read_len] = '\0';

        if(strcmp(msg, "Show the paticipants") == 0)
        {
            printf("[%02d:%02d:%02d] \"%s\" asked for a list of clients\n", time_info->tm_hour, time_info->tm_min, time_info->tm_sec, name);
            continue; 
        }
        if(strcmp(msg, "exit") == 0)
        {
            SSL_write(ssl, "bye", strlen("bye"));
            printf("[%02d:%02d:%02d] \"%s\" left\n", time_info->tm_hour, time_info->tm_min, time_info->tm_sec, name);
            break;
        }
        else
        {
            printf("[%s] %s (%02d:%02d:%02d)\n", name, msg, time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
        }
    }

    // 연결 종료 처리
    pthread_mutex_lock(&mtx);
    clnt_cnt--;
    pthread_mutex_unlock(&mtx);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(clnt_info->clnt_sock);
    free(clnt_info);

    return NULL;
}

SSL_CTX * create_context() 
{
    const SSL_METHOD * method;
    SSL_CTX * ctx;

    method = TLS_server_method();

    ctx = SSL_CTX_new(method);
    if(ctx == NULL)
    {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // Don't verify Client authentication
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    return ctx;
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


void SSL_error_handling(char * err_msg, SSL * ssl, clnt_info_pkt * clnt_info)
{
    ERR_print_errors_fp(stderr);
    printf("%s\n", err_msg);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(clnt_info->clnt_sock);
    free(clnt_info);
    pthread_mutex_lock(&mtx);
    clnt_cnt--;
    pthread_mutex_unlock(&mtx);
}
