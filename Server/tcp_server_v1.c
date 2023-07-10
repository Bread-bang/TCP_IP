#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define BUF_SIZE 1024
#define OPSZ 4
void error_handling(char * message);

int main(int argc, char * argv[])
{
	int serv_sock, clnt_sock;
	char file_list[BUF_SIZE];
	int recv_cnt, recv_len;
	struct sockaddr_in serv_adr, clnt_adr;
	socklen_t clnt_adr_sz;

	char message[] = "***\t\thello\t\t***";

	if(argc != 2)
	{
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	// 소켓 생성
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(serv_sock == -1)
		error_handling("socket() error");
	
	// 주소 정보 초기화
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htons(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));

	// 소켓에 주소 정보 할당
	if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");
	
	// 연결요청 대기상태를 위한 listen 함수
	if(listen(serv_sock, 5) == -1)
		error_handling("listen() error");
	
	// 클라이언트와 소통하기 위한 소켓의 사이즈를 담는 변수
	clnt_adr_sz = sizeof(clnt_adr);

	clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
	if(clnt_sock == -1)
		error_handling("accept() error");
	
	write(clnt_sock, message, sizeof(message));
	recv_len = read(clnt_sock, message, sizeof(message) - 1);

	if(!strcmp(message, "hello\n"))
		write(clnt_sock, "hello\n", sizeof("hello\n"));
	else
		write(clnt_sock, "bye\n", sizeof("bye\n"));

	close(serv_sock);

	return 0;
}

void error_handling(char * message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
