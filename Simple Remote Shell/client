#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/socket.h>

#define NAME_SIZE 50
#define BUF_SIZE 1024
#define PATH_SIZE 512
#define PERMISSION_SIZE 10
#define DIR_PACKET_SIZE 1072
#define FILE_PACKET_SIZE 1084
#define COLOR_BLUE "\033[38;2;0;0;255m"
#define COLOR_ORANGE "\033[38;2;255;127;80m"
#define COLOR_RESET "\033[0m"
#define SAFE_FREE(p) {if(p != NULL) {free(p); p = NULL;}}

typedef struct
{
        char file_permission_info[PERMISSION_SIZE];
        int max_link_length;
        long nlink;
        int uid;
        int gid;
        int max_file_length;
        long file_size;
        char file_name[BUF_SIZE];
}file_pkt_t;

typedef struct
{
        int path_len;
        char path[PATH_SIZE];
}path_pkt_t;

typedef struct{
    int file_size;
    int read_size;
    char name[NAME_SIZE];
    char content[BUF_SIZE]; 
}pkt_t;

void error_handling(char * message);
void setup_clnt_socket(int * sock, char * ip, int port, struct sockaddr_in * serv_adr);
void download(char * file_name, int sock, pkt_t * recv_pkt);
void upload(char * file_name, int sock, pkt_t * send_pkt);
void server_list_up(int sock, file_pkt_t * serv_file_info);
void client_list_up();
void serv_current_location(int sock, path_pkt_t * path_info);
void client_current_location();
void file_permission(struct stat * sb, char * permission);

int main(int argc, char * argv[])
{
        int i, sock;
        char buffer[BUF_SIZE];
        char cur_pwd[PATH_MAX] = ".";
        int str_len, recv_len, recv_cnt;
        struct sockaddr_in serv_adr;
        file_pkt_t * serv_file_info = (file_pkt_t*)malloc(sizeof(file_pkt_t));
        path_pkt_t * path_info = (path_pkt_t*)malloc(sizeof(path_pkt_t));
        pkt_t * recv_pkt = (pkt_t*)malloc(sizeof(pkt_t));
        pkt_t * send_pkt = (pkt_t*)malloc(sizeof(pkt_t));

        if(argc != 3)
        {
                printf("Usage : %s <IP> <port>\n", argv[0]);
                exit(0);
        }

        // Client 소켓 생성 및 초기화
        setup_clnt_socket(&sock, argv[1], atoi(argv[2]), &serv_adr);

        while(1)
        {       
                memset(buffer, 0, sizeof(buffer));
                memset(recv_pkt, 0, sizeof(pkt_t));
                memset(send_pkt, 0, sizeof(pkt_t));
                memset(path_info, 0, sizeof(path_pkt_t));
                memset(serv_file_info, 0, sizeof(file_pkt_t));

                printf("$ ");
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = '\0';

                if(!strcmp(buffer, "exit"))
                {
                        shutdown(sock, SHUT_WR);
                        break;
                }

                // 커맨드 전송
                write(sock, buffer, strlen(buffer));
                if(!strcmp(buffer, "pwd"))
                {
                        serv_current_location(sock, path_info);
                        printf("\n");
                        client_current_location();
                        continue;
                }

                if(!strcmp(buffer, "ls"))
                {
                        server_list_up(sock, serv_file_info);
                        client_list_up();
                        continue;
                }

                /*
                    ex) server cd [PATH]
                        client cd [PATH]
                */
                if(strstr(buffer, "cd") != NULL)
                {
                        char * destination = strtok(buffer, " ");
                        if(!strcmp(destination, "client"))
                        {
                                destination = strtok(NULL, " ");
                                destination = strtok(NULL, " ");
                                if(chdir(destination) != 0)
                                        error_handling("Invalid Working Directory");
                                continue;
                        }
                }

                if(strstr(buffer, "download") != NULL)
                {
                        char * download_file_name = strtok(buffer, " ");
                        download_file_name = strtok(NULL, " ");
                        download(download_file_name, sock, recv_pkt);
                        continue;
                }

                if(strstr(buffer, "upload") != NULL)
                {
                        char * upload_file_name = strtok(buffer, " ");
                        upload_file_name = strtok(NULL, " ");
                        upload(upload_file_name, sock, send_pkt);
                        continue;
                }
        }
        str_len = read(sock, buffer, BUF_SIZE);
        
        printf("Message from server : %s\n", buffer);
        free(serv_file_info);
        free(path_info);
        free(send_pkt);
        free(recv_pkt);
        close(sock);

        return 0;
}

void serv_current_location(int sock, path_pkt_t * path_info)
{
        int str_len, recv_cnt, recv_len = 0, location_len;
        char buffer[PATH_SIZE];

        while(recv_len < PATH_SIZE)
        {
                recv_cnt = read(sock, &buffer[recv_len], PATH_SIZE - recv_len);
                if(recv_cnt == -1)
                        error_handling("Failed to get the server location");
                recv_len += recv_cnt;
        }
        path_info = (path_pkt_t*)buffer;
        
        printf("%sServer Working Directory%s\n", COLOR_BLUE, COLOR_RESET);
        printf("%s\n", path_info->path);
}

void client_current_location()
{
        char path[PATH_MAX];

        if(getcwd(path, PATH_MAX) == NULL)
                error_handling("Error on pwd");
        printf("%sClient Working Directory%s\n",COLOR_ORANGE, COLOR_RESET);
        printf("%s\n", path);
}

void download(char * file_name, int sock, pkt_t * recv_pkt)
{
        int file_size;
        int recv_cnt = 0, recv_len = 0, total = 0;
        FILE * fp;
        char buffer[FILE_PACKET_SIZE];

       read(sock, &file_size, sizeof(int));

        if( (fp = fopen(file_name, "wb")) != NULL)
        {
                while(1)
                {
                        memset(buffer, 0, sizeof(buffer));
                        memset(recv_pkt, 0, sizeof(pkt_t));

                        if(total >= file_size)
                                break;

                        recv_len = 0;
                        while(recv_len < FILE_PACKET_SIZE)
                        {
                                recv_cnt = read(sock, &buffer[recv_len], FILE_PACKET_SIZE - recv_len);
                                if(recv_cnt == -1)
                                        error_handling("download error");
                                recv_len += recv_cnt;
                        }
                        recv_pkt = (pkt_t*)buffer;
                        
                        fwrite(recv_pkt->content, 1, recv_pkt->read_size, fp);
                        total += recv_pkt->read_size;
                }
        }
        else
                error_handling("File error !");

        fclose(fp);
}

void upload(char * file_name, int sock, pkt_t * send_pkt)
{
        struct stat sb;
        if(stat(file_name, &sb) == -1)
                error_handling("upload stat error");
        send_pkt->file_size = sb.st_size;
        FILE * fp;

        if((fp = fopen(file_name, "rb")) != NULL)
        {
                write(sock, &send_pkt->file_size, sizeof(int));
                while(1)
                {
                        send_pkt->read_size = fread(send_pkt->content, 1, BUF_SIZE, fp);
                        if(send_pkt->read_size < BUF_SIZE)
                        {
                                write(sock, send_pkt, sizeof(pkt_t));
                                break;
                        }
                        write(sock, send_pkt, sizeof(pkt_t));
                }
        }
        fclose(fp);        
}

void server_list_up(int sock, file_pkt_t * serv_file_info)
{
        int i, file_count, str_len, recv_len, recv_cnt;
        char buffer[DIR_PACKET_SIZE];
        
        read(sock, &file_count, sizeof(int));

        printf("%sServer Directory%s\n", COLOR_BLUE, COLOR_RESET);
        for(i = 0; i < file_count; i++)
        {
                memset(serv_file_info, 0, sizeof(file_pkt_t));
                recv_len = 0;
                while(recv_len < DIR_PACKET_SIZE)
                {
                        recv_cnt = read(sock, &buffer[recv_len], DIR_PACKET_SIZE - recv_len);
                        if(recv_cnt == -1)
                                        error_handling("server ls failed");
                        recv_len += recv_cnt;
                }
                serv_file_info = (file_pkt_t*)buffer;
                // str_len = read(sock, serv_file_info, sizeof(file_pkt_t));
                // printf("%d\n", str_len);
                printf("%s %*ld %d %d %*ld %s\n", serv_file_info->file_permission_info, serv_file_info->max_link_length, serv_file_info->nlink, serv_file_info->uid, serv_file_info->gid, serv_file_info->max_file_length, serv_file_info->file_size, serv_file_info->file_name);
        }
}

void client_list_up() 
{
        int i, file_count;
        int max_file_length = 0, max_link_length = 0;
        char permission[PERMISSION_SIZE];
        DIR * dp;
        struct dirent * dir;
        struct dirent ** file_list;
        struct stat sb;

        printf("%sClient Directory%s\n",COLOR_ORANGE, COLOR_RESET);
        if((dp = opendir(".")) != NULL)
        {
                while((dir = readdir(dp)) != NULL)
                {
                if(stat(dir->d_name, &sb) == -1)
                {
                        printf("%s stat error\n", dir->d_name);
                        exit(-1);
                }
                int file_size_length = snprintf(NULL, 0, "%lld", (long long)sb.st_size);
                int file_link_length = snprintf(NULL, 0, "%ld", sb.st_nlink);
                max_file_length = (file_size_length > max_file_length) ? file_size_length : max_file_length;
                max_link_length = (file_link_length > max_link_length) ? file_link_length : max_link_length;
                }
        }

        rewinddir(dp);

        file_count = scandir(".", &file_list, NULL, alphasort);
        for(i = 0; i < file_count; i++)
        {
                if(stat(file_list[i]->d_name, &sb) == -1)
                {
                        printf("%s stat error\n", file_list[i]->d_name);
                        exit(-1);
                }

                file_permission(&sb, permission);
                printf("%s %*ld %d %d %*ld %s\n", permission, max_link_length, sb.st_nlink, sb.st_uid, sb.st_gid, max_file_length, sb.st_size, file_list[i]->d_name);

                SAFE_FREE(file_list[i]);
                memset(permission, 0, sizeof(permission));
        }
        SAFE_FREE(file_list);
        closedir(dp);
}
void file_permission(struct stat * sb, char * permission)
{
    memset(permission, 0, sizeof(permission));
    strcat(permission, (S_ISDIR(sb->st_mode)) ? "d" : "-");
    strcat(permission, (sb->st_mode & S_IRUSR) ? "r" : "-");
    strcat(permission, (sb->st_mode & S_IWUSR) ? "w" : "-");
    strcat(permission, (sb->st_mode & S_IXUSR) ? "x" : "-");
    strcat(permission, (sb->st_mode & S_IRGRP) ? "r" : "-");
    strcat(permission, (sb->st_mode & S_IWGRP) ? "w" : "-");
    strcat(permission, (sb->st_mode & S_IXGRP) ? "x" : "-");
    strcat(permission, (sb->st_mode & S_IROTH) ? "r" : "-");
    strcat(permission, (sb->st_mode & S_IWOTH) ? "w" : "-");
    strcat(permission, (sb->st_mode & S_IXOTH) ? "x" : "-");
}

void setup_clnt_socket(int * sock, char * ip, int port, struct sockaddr_in * serv_adr)
{
        *sock = socket(PF_INET, SOCK_STREAM, 0);
        if(*sock == -1)
                error_handling("socket() error");

        memset(serv_adr, 0, sizeof(*serv_adr));
        serv_adr->sin_family = AF_INET;
        serv_adr->sin_addr.s_addr = inet_addr(ip);
        serv_adr->sin_port = htons(port);

        if(connect(*sock, (struct sockaddr*)serv_adr, sizeof(*serv_adr)) == -1)
                error_handling("connect() error");
}

void error_handling(char * message)
{
        fputs(message, stderr);
        fputc('\n', stderr);
        exit(0);
}

