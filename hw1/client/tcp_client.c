#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024
#define NUM_BUF_SIZE 5

void error_handling(char * message);
void setup_clnt_socket(int * sock, char * ip, int port, struct sockaddr_in * serv_adr);
void download_file(char * file_name, int sock);

int main(int argc, char * argv[])
{
	int sock;
	char buffer[BUF_SIZE];
	char num_message[NUM_BUF_SIZE];
	int str_len, recv_len, recv_cnt;
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
		memset(buffer, 0, sizeof(buffer));
		str_len = read(sock, buffer, sizeof(buffer) - 1);
		if(str_len == -1)
			error_handling("read() error!");

		printf("%s ", buffer);
		scanf("%s", num_message);

		// 파일 번호 전송
		str_len = write(sock, num_message, strlen(num_message) + 1);

		if(!strcmp(num_message, "q") || !strcmp(num_message, "Q"))
		{
			close(sock);
			break;
		}

		// 파일 이름 받기
		recv_len = 0;
		str_len = read(sock, buffer, sizeof(buffer) - 1);
		if(str_len == -1)
			error_handling("read() error!");

		// file 다운로드
		download_file(buffer, sock);
	}
	return 0;
}

void download_file(char * file_name, int sock)
{
	FILE * fp;
	char buffer[BUF_SIZE];

	if( (fp = fopen(file_name, "wb")) != NULL)
	{
		while(1)
		{
			memset(buffer, 0, sizeof(char));
			int bytes_read = read(sock, buffer, BUF_SIZE);

 			if(bytes_read > 0)
                                fwrite(buffer, bytes_read, 1, fp);

                        if(bytes_read < BUF_SIZE)
                        {
                                printf("\n\t\t\t => %s downloaded", file_name);
                                break;
                        }
		}
	}	
	else
		error_handling("File error !");

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
