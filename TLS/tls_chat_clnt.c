#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/stat.h>
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
void ls_command();
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
                             "2. ls\n"
                             "3. exit\n";
    printf("%s", direction);

    pthread_create(&send_thread, NULL, send_msg, (void*)&clnt_info);
    pthread_create(&recv_thread, NULL, recv_msg, (void*)&clnt_info);
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
    clnt_info_pkt * clnt_info;
    SSL * ssl;
    char name[NAME_SIZE];

    clnt_info = (clnt_info_pkt*)arg;
    ssl = clnt_info->ssl;
    strcpy(name, clnt_info->name);

    char * command = (char *)malloc(BUF_SIZE);
    while(1)
    {
        usleep(100000); // For alignment
        // printf("[%s] ", name);

        fgets(command, BUF_SIZE, stdin);
        command[strlen(command) - 1] = '\0';

        if(strcmp(command, "1") == 0)
        {
            SSL_write(ssl, "Show the paticipants", strlen("Show the paticipants"));
            usleep(100000); // For alignment
            continue;
        }
        else if(strcmp(command, "2") == 0 || strcmp(command, "ls") == 0)
        {
            ls_command();
            continue;
        }
        else if(strcmp(command, "3") == 0 || strcmp(command, "exit") == 0)
        {
            SSL_write(ssl, "exit", strlen("exit"));
            break;
        }
        else
        {
            if(strncmp(command, "file_share:", 11) == 0)
            {

                SSL_write(ssl, "file_share", strlen("file_share"));

                // Send file name
                const char * file_name = command + 11;
                printf("file_name : %s\n", file_name);
                SSL_write(ssl, file_name, strlen(file_name));

                // Send file size
                struct stat sb;
                stat(file_name, &sb);
                int file_size = sb.st_size;
                SSL_write(ssl, &file_size, sizeof(file_size));

                FILE * fp;
                if((fp = fopen(file_name, "rb")) != NULL)
                {
                    while(1)
                    {
                        char buf[BUF_SIZE];
                        memset(buf, 0, BUF_SIZE);

                        int read_len = fread(buf, 1, BUF_SIZE, fp);
                        if(read_len < BUF_SIZE)
                        {
                            SSL_write(ssl, buf, read_len);
                            break;
                        }
                        SSL_write(ssl, buf, BUF_SIZE);
                    }
                    fclose(fp);
                }
                else
                {
                    printf("File open error\n");
                }
            }
            else
            {
                SSL_write(ssl, command, strlen(command));
            }
        }
    }
    return NULL;
}

void * recv_msg(void * arg)
{
    clnt_info_pkt * clnt_info;
    SSL * ssl;
    int str_len;
    char msg[BUF_SIZE];

    clnt_info = (clnt_info_pkt*)arg;
    ssl = clnt_info->ssl;

    while(1)
    {
        memset(msg, 0, BUF_SIZE);
        fflush(stdout);
        str_len = SSL_read(ssl, msg, BUF_SIZE - 1);
        if(str_len <= 0)
            return NULL;

        if(strcmp(msg, "List") == 0)
        {
            printf("-----------------------------------\n");
            printf("\tList of participants\n");
            printf("-----------------------------------\n");
            fflush(stdout);
            continue;
        }
        else if(strstr(msg, "new_client") != NULL)
        {
            char *sep = " ";
            char *word, *brkt, *brkb;
            char info[2][BUF_SIZE];

            word = strtok_r(msg, sep, &brkt);
            word = strtok_r(NULL, sep, &brkt);
            for(int i = 0; i < 2; i++) 
            {
                strcpy(info[i], word);
                word = strtok_r(NULL, sep, &brkt);
            }

            printf("%s %s joined\n", info[1], info[0]);
            fflush(stdout);

        }
        else if(strcmp(msg, "file_share") == 0)
        {
            char file_name[BUF_SIZE];
            SSL_read(ssl, file_name, BUF_SIZE);

            int file_size;
            SSL_read(ssl, &file_size, sizeof(file_size));

            FILE * fp;
            if((fp = fopen(file_name, "wb")) != NULL)
            {
                char buf[BUF_SIZE];
                while(1)
                {
                    int read_len;
                    read_len = SSL_read(ssl, buf, BUF_SIZE - 1);
                    if(read_len <= 0) 
                        break;
                    fwrite(buf, 1, read_len, fp);
                    file_size -= read_len;
                    if(file_size <= 0)
                        break;
                }
                fclose(fp);
            }
            else
            {
                error_handling("Failed to download the file");
            }
        }
        else
        {
            msg[str_len] = '\0';
            printf("%s", msg);
            fflush(stdout);
        }
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

void ls_command()
{
    DIR * dp;
    struct dirent * dir;
    struct stat sb;

    if((dp = opendir(".")) == NULL)
    {
        perror("directory open error\n");
        exit(1);
    }

    while((dir = readdir(dp)) != NULL)
    {
        if(stat(dir->d_name, &sb) == -1)
        {
            printf("%s stat error\n", dir->d_name);
            exit(-1);
        }

        if((dir->d_ino == 0) || !(strcmp(dir->d_name, ".")) || !(strcmp(dir->d_name,"..")) || !(strcmp(dir->d_name, ".DS_Store")))
            continue;

        if(!S_ISREG(sb.st_mode))
            continue;

        // File type
        printf("%c%c%c%c%c%c%c%c%c%c ",
            (S_ISDIR(sb.st_mode)) ? 'd' : '-',
            (sb.st_mode & S_IRUSR) ? 'r' : '-',
            (sb.st_mode & S_IWUSR) ? 'w' : '-',
            (sb.st_mode & S_IXUSR) ? 'x' : '-',
            (sb.st_mode & S_IRGRP) ? 'r' : '-',
            (sb.st_mode & S_IWGRP) ? 'w' : '-',
            (sb.st_mode & S_IXGRP) ? 'x' : '-',
            (sb.st_mode & S_IROTH) ? 'r' : '-',
            (sb.st_mode & S_IWOTH) ? 'w' : '-',
            (sb.st_mode & S_IXOTH) ? 'x' : '-');
        
        // File link count
        printf("%3d ", (int)sb.st_nlink);

        // File owner & group
        printf("%s %s ", getpwuid(sb.st_uid)->pw_name, getgrgid(sb.st_gid)->gr_name);
        printf("%8lld ", (long long)sb.st_size);

        // Creation time
        char time_str[26];
        strftime(time_str, sizeof(time_str), "%b %d %H:%M", localtime(&sb.st_mtime));
        printf("%s ", time_str);

        // File name
        printf("%s\n", dir->d_name);
    }
}

void error_handling(char * msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}