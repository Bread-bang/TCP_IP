#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024
#define NUM_BUF_SIZE 5

typedef struct{
    int idx;
    int file_size;
    int pkt_size;
    char name[BUF_SIZE]; 
    char content[BUF_SIZE];
}pkt_t;

void error_handling(char * message);
void setup_socket(int * serv_sock, int port, struct sockaddr_in * serv_adr);
void make_message(char * message, char file_list[][BUF_SIZE], int * file_list_size, int clnt_sock, pkt_t * send_pkt);
void upload_file(int file_cnt, char * file_num, char file_list[][BUF_SIZE], int * file_list_size, int clnt_sock, pkt_t * send_pkt);
int list_up(char file_list[][BUF_SIZE], int * file_list_size);


int main(int argc, char * argv[])
{
    int serv_sock, clnt_sock;
    int recv_cnt, recv_len;
	int file_cnt;
    struct sockaddr_in serv_adr, clnt_adr;
    pkt_t * send_pkt = (pkt_t*)malloc(sizeof(pkt_t)); 
    socklen_t clnt_adr_sz;
    char file_list[BUF_SIZE][BUF_SIZE];
    int file_list_size[BUF_SIZE];
    char buffer[BUF_SIZE];
    char selected[BUF_SIZE];

    if(argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // Socket 생성 관련 함수 
    setup_socket(&serv_sock, atoi(argv[1]), &serv_adr);

    while(1)
    {
        // Accept 함수 호출로 대기 큐에서 대기 중인 첫 번째 클라이언트와의 연결을 구성
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
        if(clnt_sock == -1)
            error_handling("accept() error");
        else
            fputs("Client connected \n", stdout);

        while(1)
        {
            memset(send_pkt, 0, sizeof(pkt_t));
			memset(buffer, 0, sizeof(buffer));

            // 배열에 디렉토리 파일 정보 저장
            file_cnt = list_up(file_list, file_list_size); 

            // 가이드 메시지 전송 
            make_message(buffer, file_list, file_list_size, clnt_sock, send_pkt);
			
            // Client 번호 읽기
            char file_num[NUM_BUF_SIZE] = "";
            recv_len = read(clnt_sock, file_num, NUM_BUF_SIZE - 1);
			if(recv_len == 0)
			{
				close(clnt_sock);
				break;
			}
            if(recv_len == -1)
                error_handling("Reading number from client error!");
            file_num[recv_len] = '\0';

            if( !strcmp(file_num, "q") || !strcmp(file_num, "Q") )
            {
                fputs("Disconnected\n", stdout);
                close(clnt_sock);
                memset(buffer, 0, sizeof(char));
                break;
            }

			memset(send_pkt, 0, sizeof(pkt_t));

            // file 전송
            upload_file(file_cnt, file_num, file_list, file_list_size, clnt_sock, send_pkt);
            
        }
    }
	free(send_pkt);
    close(serv_sock);

    return 0;
}

void upload_file(int file_cnt, char * file_num, char file_list[][BUF_SIZE], int * file_list_size, int clnt_sock, pkt_t * send_pkt)
{
	int total = 0, recv_size;

	send_pkt->idx = file_cnt;
	send_pkt->file_size = file_list_size[atoi(file_num)];
	strcpy(send_pkt->name, file_list[atoi(file_num)]);

	write(clnt_sock, send_pkt, sizeof(pkt_t));
	
	char buffer[BUF_SIZE];
	
    FILE * fp;
    if( (fp = fopen(file_list[atoi(file_num)], "rb")) != NULL)
    {
		while(1)
		{
			send_pkt->pkt_size = fread(send_pkt->content, 1, BUF_SIZE, fp);
			total += send_pkt->pkt_size;
			write(clnt_sock, send_pkt, sizeof(pkt_t));
			if(total == send_pkt->file_size)
                break;
				
		}
		printf("\t%s uploaded\n", send_pkt->name);
    }
    else
        error_handling("File open error!");

    fclose(fp);
}

int list_up(char file_list[][BUF_SIZE], int * file_list_size)
{
    DIR * dp;
    struct dirent * dir;
    struct stat sb;

    if((dp = opendir(".")) == NULL)
    {
        perror("directory open error\n");
        exit(1);
    }

    int cnt = 1;
    int max_len = 0;
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

        // 파일 이름 
        strcpy(file_list[cnt], dir->d_name);

        // 파일 크기
        file_list_size[cnt++] = sb.st_size;
    }

	return --cnt;
}

void make_message(char * message, char file_list[][BUF_SIZE], int * file_list_size, int clnt_sock, pkt_t * send_pkt)
{
    int i = 1;
	int str_len;
    while(file_list[i][0] != '\0')
    {
        send_pkt->idx = i;
        send_pkt->file_size = file_list_size[i];
        strcpy(send_pkt->name, file_list[i]);
    	write(clnt_sock, send_pkt, sizeof(pkt_t));
        i++;
    }
    send_pkt->idx = -1;
    write(clnt_sock, send_pkt, sizeof(pkt_t));
}

void setup_socket(int * serv_sock, int port, struct sockaddr_in * serv_adr)
{
    // 소켓 생성
    *serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(*serv_sock == -1)
        error_handling("socket() error");

    // 주소 정보 초기화
    memset(serv_adr, 0, sizeof(*serv_adr));
    serv_adr->sin_family = AF_INET;
    serv_adr->sin_addr.s_addr = htons(INADDR_ANY);
    serv_adr->sin_port = htons(port);

    // 소켓에 주소 정보 할당
    if(bind(*serv_sock, (struct sockaddr*)serv_adr, sizeof(*serv_adr)) == -1)
        error_handling("bind() error");

    // 연결요청 대기상태를 위한 listen 함수
    if(listen(*serv_sock, 5) == -1)
        error_handling("listen() error");

}

void error_handling(char * message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
