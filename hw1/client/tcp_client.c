#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024 
#define PACKET_SIZE 2060
#define NUM_BUF_SIZE 5

typedef struct{
    int idx;
    int file_size;
    int pkt_size;
    char name[BUF_SIZE];
    char content[BUF_SIZE];    
}pkt_t;

void error_handling(char * message);
void setup_clnt_socket(int * sock, char * ip, int port, struct sockaddr_in * serv_adr);
void download_file(int sock, pkt_t * recv_pkt);

int main(int argc, char * argv[])
{
    int sock;
    char buffer[BUF_SIZE];
    char temp[PACKET_SIZE];
    char num_message[NUM_BUF_SIZE];
    int recv_len, recv_cnt;
	pkt_t * recv_pkt = (pkt_t *)malloc(sizeof(pkt_t)); 
    struct sockaddr_in serv_adr;
    

    if(argc != 3)
    {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(0);
    }

	// Client 소켓 생성 및 초기화
   	setup_clnt_socket(&sock, argv[1], atoi(argv[2]), &serv_adr); 

    while(1)
    {
        printf("\n\n\t\t*** 현재 디렉토리 파일 목록 ***\t\t\n\n");
        while(1)
        {
            memset(temp, 0, sizeof(temp));
            memset(recv_pkt, 0, sizeof(pkt_t));
            recv_len = 0;
            while(recv_len < PACKET_SIZE)
            {	
                recv_cnt = read(sock, &temp[recv_len], PACKET_SIZE - recv_len);
                if(recv_cnt == -1)
                    error_handling("read() error!");
                recv_len += recv_cnt;
            }
            recv_pkt = (pkt_t*)temp;

			if(recv_pkt->idx == -1)
				break;
				
            printf("\t %d \t %s \t\t\t\t\t%d\n", recv_pkt->idx, recv_pkt->name, recv_pkt->file_size);
        }

        printf("\n\t> 다운로드 할 파일을 선택하세요(q to quit) : ");
        scanf("%s", num_message);
        
        // 파일 번호 전송
        recv_cnt = write(sock, num_message, strlen(num_message) + 1);

        if(!strcmp(num_message, "q") || !strcmp(num_message, "Q"))
        {
            close(sock);
            break;
        }

        // file 다운로드
        download_file(sock, recv_pkt);
    }
    return 0;
}

void download_file(int sock, pkt_t * recv_pkt)
{
	int recv_size = 0, recv_len = 0, total = 0, file_size = 0;
    char file_name[BUF_SIZE];
	char buffer[PACKET_SIZE];
	FILE * fp;

	read(sock, recv_pkt, sizeof(pkt_t));    
    file_size = recv_pkt->file_size;
    strcpy(file_name, recv_pkt->name);

    if( (fp = fopen(recv_pkt->name, "wb")) != NULL)
    {
		while(1)
		{
            memset(buffer, 0, sizeof(buffer));
            memset(recv_pkt, 0, sizeof(pkt_t));

            if(total >= file_size)
                break;

			recv_len = 0;
            while(recv_len < PACKET_SIZE)
            {
                recv_size = read(sock, &buffer[recv_len], PACKET_SIZE - recv_len);
                if(recv_size == -1)
                    error_handling("download failed");
                recv_len += recv_size;
            }
            recv_pkt = (pkt_t*)buffer;

            fwrite(recv_pkt->content, 1, recv_pkt->pkt_size, fp);
            total += recv_pkt->pkt_size;
		}
        printf("\n\t\t\t=> %s downloaded\n", file_name);
    }   
    else
        error_handling("File error !");

    memset(buffer, 0, sizeof(buffer));
    fclose(fp);
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

