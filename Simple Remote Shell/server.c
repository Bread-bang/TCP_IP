#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#define NAME_SIZE 50
#define BUF_SIZE 1024
#define PATH_SIZE 512
#define PERMISSION_SIZE 10
#define DIR_PACKET_SIZE 1072
#define FILE_PACKET_SIZE 1084

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

typedef struct
{
        int file_size;
        int read_size;
        char name[NAME_SIZE];
        char content[BUF_SIZE]; 
}pkt_t;


void error_handling(char * message);
void setup_sock(int * serv_sock, int port, struct sockaddr_in * serv_adr);
void server_list_up(int clnt_sock, file_pkt_t * serv_file_info);
void client_list_up();
void file_permission(struct stat * sb, char * permission);
void current_location(int clnt_sock, path_pkt_t * path_info);
void upload_file(int clnt_sock, char * file_name, pkt_t * send_pkt);
void download_file(int clnt_sock, char * file_name, pkt_t * recv_pkt);

int main(int argc, char * argv[])
{
        int serv_sock, clnt_sock;
        int fd_max, str_len, fd_num, i, recv_len, recv_cnt;
        char message[BUF_SIZE];

        struct sockaddr_in serv_adr, clnt_adr;
        struct timeval timeout;
        file_pkt_t * serv_file_info = (file_pkt_t*)malloc(sizeof(file_pkt_t));
        path_pkt_t * path_info = (path_pkt_t*)malloc(sizeof(path_pkt_t));
        pkt_t * send_pkt = (pkt_t*)malloc(sizeof(pkt_t));
        pkt_t * recv_pkt = (pkt_t*)malloc(sizeof(pkt_t));
        fd_set reads, cpy_reads;

        socklen_t adr_sz;

        if(argc != 2)
        {
        printf("Usage : %s <port> \n", argv[0]);
        exit(1);
        }

        setup_sock(&serv_sock, atoi(argv[1]), &serv_adr);

        FD_ZERO(&reads);
        FD_SET(serv_sock, &reads);
        fd_max = serv_sock;

        while(1)
        {
                cpy_reads = reads;
                timeout.tv_sec = 5;
                timeout.tv_usec = 5000;

                if((fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout)) == -1)
                        break;

                // Timeout 발생
                if(fd_num == 0)
                        continue;

                for(i = 0; i < fd_max + 1; i++)
                {
                        if(FD_ISSET(i, &cpy_reads))
                        {
                                if(i == serv_sock)
                                {
                                        adr_sz = sizeof(clnt_adr);
                                        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
                                        FD_SET(clnt_sock, &reads);
                                        if(fd_max < clnt_sock)
                                        fd_max = clnt_sock;
                                        printf("Connected client : %d\n", clnt_sock);
                                }
                                else
                                {
                                        memset(message, 0, sizeof(message));
                                        str_len = read(i, message, BUF_SIZE);
                                        
                                        if(str_len == 0)
                                        {
                                                char temp[13] = "See you soon";
                                                write(i, temp, strlen(temp));
                                                FD_CLR(i, &reads);
                                                close(i);
                                                printf("Closed client : %d\n", i);
                                        }
                                        else
                                        {
                                                if(!(strcmp(message, "ls")))
                                                {
                                                        server_list_up(i, serv_file_info);
                                                        continue;
                                                }
                                                
                                                if(!(strcmp(message, "pwd")))
                                                {
                                                        current_location(i, path_info);
                                                        continue;
                                                }

                                                if(strstr(message, "download") != NULL)
                                                {
                                                        // 파일 이름 
                                                        char * download_file_name = strtok(message, " ");
                                                        download_file_name = strtok(NULL, " ");
                                                        upload_file(i, download_file_name, send_pkt);
                                                        continue;
                                                }
                                                
                                                if(strstr(message, "upload") != NULL)
                                                {
                                                        char * upload_file_name = strtok(message, " ");
                                                        upload_file_name = strtok(NULL, " ");
                                                        download_file(i, upload_file_name, recv_pkt);
                                                        continue;
                                                }

                                                if(strncmp(message, "server", 6) == 0)
                                                {
                                                        char * path = message + 10;
                                                        printf("path : %s\n", path);
                                                        if(chdir(path) != 0)
                                                                error_handling("server cd error!");
                                                        continue;
                                                }
                                        }
                                }
                        }
                }
        }
        free(serv_file_info);
        free(path_info);
        free(send_pkt);
        free(recv_pkt);
        close(serv_sock);
        return 0;
}

void setup_sock(int * serv_sock, int port, struct sockaddr_in * serv_adr)
{
        *serv_sock = socket(PF_INET, SOCK_STREAM, 0);
        if(*serv_sock == -1)
                error_handling("socket() error");

        memset(serv_adr, 0, sizeof(*serv_adr));
        serv_adr->sin_family = AF_INET;
        serv_adr->sin_addr.s_addr = htons(INADDR_ANY);
        serv_adr->sin_port = htons(port);

        if(bind(*serv_sock, (struct sockaddr*)serv_adr, sizeof(*serv_adr)) == -1)
                error_handling("bind() error");

        if(listen(*serv_sock, 5) == -1)
                error_handling("listen() error");
}

void server_list_up(int clnt_sock, file_pkt_t * serv_file_info)
{
        int i, file_count, str_len;
        int max_file_length = 0, max_link_length = 0;
        char permission[PERMISSION_SIZE];
        DIR * dp;
        struct dirent * dir;
        struct dirent ** file_list;
        struct stat sb;

        memset(serv_file_info, 0, sizeof(file_pkt_t));
        memset(permission, 0, sizeof(permission));

        if((dp = opendir(".")) != NULL)
        {
                while((dir = readdir(dp)) != NULL)
                {
                        if(stat(dir->d_name, &sb) == -1)
                        {
                                printf("%s stat error\n", dir->d_name);
                                continue;
                        }
                        int file_size_length = snprintf(NULL, 0, "%lld", (long long)sb.st_size);
                        int file_link_length = snprintf(NULL, 0, "%ld", sb.st_nlink);
                        max_file_length = (file_size_length > max_file_length) ? file_size_length : max_file_length;
                        max_link_length = (file_link_length > max_link_length) ? file_link_length : max_link_length;
                }
                serv_file_info->max_file_length = max_file_length;
                serv_file_info->max_link_length = max_link_length;
        }

        rewinddir(dp);

        file_count = scandir(".", &file_list, NULL, alphasort);
        write(clnt_sock, &file_count, sizeof(int));

        for(i = 0; i < file_count; i++)
        {
                if(stat(file_list[i]->d_name, &sb) == -1)
                {
                        printf("%s stat error\n", file_list[i]->d_name);
                        continue;
                }

                file_permission(&sb, permission);

                strcpy(serv_file_info->file_permission_info, permission);
                serv_file_info->nlink = sb.st_nlink;
                serv_file_info->uid = sb.st_uid;
                serv_file_info->gid = sb.st_gid;
                serv_file_info->file_size = sb.st_size;
                strcpy(serv_file_info->file_name, file_list[i]->d_name);

                write(clnt_sock, serv_file_info, sizeof(file_pkt_t));
                // printf("%s %*ld %d %d %*ld %s\n", serv_file_info->file_permission_info, serv_file_info->max_link_length, serv_file_info->nlink, serv_file_info->uid, serv_file_info->gid, serv_file_info->max_file_length, serv_file_info->file_size, serv_file_info->file_name);

                SAFE_FREE(file_list[i]);
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

void current_location(int clnt_sock, path_pkt_t * path_info)
{
        getcwd(path_info->path, PATH_SIZE);
        path_info->path_len= strlen(path_info->path);
        printf("len: %d\n", path_info->path_len);
        printf("path : %s\n", path_info->path);
        printf("location bytes : %ld\n", write(clnt_sock, path_info, PATH_SIZE));
}

void download_file(int clnt_sock, char * file_name, pkt_t * recv_pkt)
{
        int file_size;
        int recv_cnt = 0, recv_len = 0, total = 0;
        FILE * fp;
        char buffer[FILE_PACKET_SIZE];

        read(clnt_sock, &file_size, sizeof(int));

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
                                recv_cnt = read(clnt_sock, &buffer[recv_len], FILE_PACKET_SIZE - recv_len);
                                if(recv_cnt == -1)
                                        error_handling("download failed");
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

void upload_file(int clnt_sock, char * file_name, pkt_t * send_pkt)
{
        int str_len;
        char buffer[BUF_SIZE];
        struct stat sb;
        FILE * fp;

        if(stat(file_name, &sb) == -1)
            error_handling("upload error");

        send_pkt->file_size = sb.st_size;
        if((fp = fopen(file_name, "rb")) != NULL)
        {
                write(clnt_sock, &send_pkt->file_size, sizeof(int));
                
                str_len= 0;
                while(1)
                {
                        send_pkt->read_size = fread(send_pkt->content, 1, BUF_SIZE, fp);
                        if(send_pkt->read_size < BUF_SIZE)
                        {   
                                write(clnt_sock, send_pkt, sizeof(pkt_t));
                                break;
                        }
                        write(clnt_sock, send_pkt, sizeof(pkt_t));
                        str_len += send_pkt->read_size;
                }
                printf("%d\n", str_len);
        }
        fclose(fp);
}

void error_handling(char * buf)
{
        fputs(buf, stderr);
        fputc('\n', stderr);
        exit(1);
}
