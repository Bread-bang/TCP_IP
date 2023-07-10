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

void error_handling(char * message);
void setup_socket(int * serv_sock, int port, struct sockaddr_in * serv_adr);
void make_message(char * message, char file_list[][BUF_SIZE], char file_list_size[][BUF_SIZE], int fname_len);
void upload_file(char * file_name, char * buffer, int clnt_sock);
int list_up(char file_list[][BUF_SIZE], char file_list_size[][BUF_SIZE]);

int main(int argc, char * argv[])
{
	int serv_sock, clnt_sock;
	int recv_cnt, recv_len;
	struct sockaddr_in serv_adr, clnt_adr;
	socklen_t clnt_adr_sz;
	char file_list[BUF_SIZE][BUF_SIZE];
	char file_list_size[BUF_SIZE][BUF_SIZE];
	char buffer[BUF_SIZE];
	char file_name[BUF_SIZE];
	int size, fname_len;

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
			// 파일 리스트업, 가장 긴 이름의 파일 알아내기
			fname_len = list_up(file_list, file_list_size);	

			memset(buffer, 0, sizeof(buffer));	

			// 가이드 메시지 만들기
			strcat(buffer, "\n\n\t\t*** 현재 디렉토리 파일 목록 ***\t\t\n\n");
			make_message(buffer, file_list, file_list_size, fname_len);	

			// 가이드 메시지 전송
			write(clnt_sock, buffer, strlen(buffer) + 1);

			// Client 번호 읽기
			char file_num[NUM_BUF_SIZE] = "";
			recv_len = read(clnt_sock, file_num, NUM_BUF_SIZE - 1);
			if(recv_len == -1)
				error_handling("Reading number from client error!");
			file_num[recv_len] = '\0';

			if( !strcmp(file_num, "q") || !strcmp(file_num, "Q") )
			{
				fputs("disconnected\n", stdout);
				close(clnt_sock);
				memset(buffer, 0, sizeof(char));
				break;
			}

			// 해당 번호 파일 이름
			char selected[BUF_SIZE];
			strcpy(selected, file_list[atoi(file_num) - 1]);

			// file 이름 전송
			write(clnt_sock, selected, strlen(selected) + 1);

			// file 전송
			upload_file(selected, buffer, clnt_sock);
		}
	}
	close(serv_sock);

	return 0;
}

void upload_file(char * file_name, char *  buffer, int clnt_sock)
{
	FILE * fp;
	if( (fp = fopen(file_name, "rb")) != NULL)
	{
		while(1)
		{
			memset(buffer, 0, sizeof(char));
			int bytes_read = fread(buffer, sizeof(char), BUF_SIZE, fp);

			// 읽어온 데이터가 있다면
			if(bytes_read > 0)
				write(clnt_sock, buffer, bytes_read);

			// BUF_SIZE 만큼 못 읽어 왔을 경우
			if(bytes_read < BUF_SIZE)
			{	
				// 1.파일을 다 읽었을 때
				if(feof(fp))
					printf("%s uploaded.\n", file_name);

				// 2. 읽기 실패했을 때
				if(ferror(fp))
					error_handling("Error reading");
				break;
			}
		}
	}
	else
		error_handling("File open error!");

	fclose(fp);
}

int list_up(char file_list[][BUF_SIZE], char file_list_size[][BUF_SIZE])
{
	DIR *dp;
	struct dirent *dir;
	struct stat sb;

	if((dp = opendir(".")) == NULL)
	{
		perror("directory open error\n");
		exit(1);
	}

	int cnt = 0;
	int max_len = 0;
	while((dir = readdir(dp)) != NULL)
	{
		if(stat(dir->d_name, &sb) == -1)
		{
			printf("%s stat error\n", dir->d_name);
			exit(-1);
		}

		if((dir->d_ino == 0) || !(strcmp(dir->d_name, ".")) || !(strcmp(dir->d_name,"..")) || !(strcmp(dir->d_name, ".DS_Store")) || !(strcmp(dir->d_name, __FILE__)))
			continue;

		if(!S_ISREG(sb.st_mode))
			continue;

		// 파일 이름 
		strcpy(file_list[cnt], dir->d_name);
		// 파일 크기
		char file_size[20];
		sprintf(file_size, "%lld", (long long) sb.st_size);
		strcpy(file_list_size[cnt], file_size);

		// 파일 이름 최대 길이
		max_len = (max_len < strlen(dir->d_name)) ? strlen(dir->d_name) : max_len; 
		cnt++;
	}

	return max_len;
}

void make_message(char * message, char file_list[][BUF_SIZE], char file_list_size[][BUF_SIZE], int fname_len)
{
	int i = 0, cnt = 1;
	while(file_list[i][0] != '\0')
	{
		char line[BUF_SIZE];
		sprintf(line, "\t\t%-5d%-40s\t%10s\n", cnt++, file_list[i], file_list_size[i]);
		strcat(message, line);
		i++;
	}
	strcat(message, "\n\t\t> 다운로드 할 파일을 선택하세요(q to quit) :");
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
