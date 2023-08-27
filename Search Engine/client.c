#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <termios.h>

#define LINE 40
#define QUERY_SIZE 100
#define BUF_SIZE 1024
#define PACKET_SIZE 48

#define COLOR_GREEN "\033[38;2;0;229;38m"
#define COLOR_ORANGE "\33[38;2;255;77;0m"
#define COLOR_RESET "\033[0m"

typedef struct {
    int whole_cnt;
    int search_time;
    char search_word[LINE];
} db_pkt_t;

void setup_clnt_socket(int * sock, char * ip, int port, struct sockaddr_in * serv_adr);
void toLowerCase(char *str);
void error_handling(char * message);
int getch();

int main(int argc, char *argv[]) {
    int sock, i, j, str_len, count = 0;
    char msg[BUF_SIZE];
    char c;
    char query[100] = "";
    int position = 0;
    struct sockaddr_in serv_adr;

    db_pkt_t search_list;

    // Client 소켓 생성 및 초기화
    setup_clnt_socket(&sock, argv[1], atoi(argv[2]), &serv_adr);

    // 가이드 메시지 출력
    printf("%sSearch Word:%s %s\n", COLOR_GREEN, COLOR_RESET, query);
    printf("----------------------\n");

    // 커서 위치 설정
    int to_right_cursor = position + strlen("Search Word: ");
    printf("\033[%dA", 2);
    printf("\033[%dC", to_right_cursor);

    while (1) {
        c = getch();

        if (c == '\n') 
        {
            shutdown(sock, SHUT_WR);
            read(sock, msg, sizeof(int));
            printf("Server from message : %s\n", msg);
            break; 
        }
        if (c == 127 && position > 0) 
        { 
            query[--position] = '\0';
            printf("\033[1D"); 
            printf(" ");
        } 
        else if(c == 127 && position <= 0)
        {
            continue;
        }
        else if (isprint(c)) 
        {
            query[position++] = c;
            query[position] = '\0';
        }

        printf("\033[0G");
        printf("%sSearch Word:%s %s\n", COLOR_GREEN, COLOR_RESET, query);
        printf("----------------------\n");

        if(count != 1)
        {
            for(i = 0; i < count - 1; i++)
            {
                printf("\033[K");
                printf("\033[1B");
            }
            printf("\033[K");
            printf("\033[0G");
            printf("\033[%dA", count - 1);
        }
        else
        {
            printf("\033[K");
            printf("\033[0G");
        }
        
        str_len = write(sock, query, QUERY_SIZE);
        read(sock, &count, sizeof(int));

        int recv_len, recv_cnt;
        for (i = 0; i < count; i++) 
        {
            char temp[PACKET_SIZE];
            recv_len = 0;
            while (recv_len < PACKET_SIZE) 
            {
                recv_cnt = read(sock, &temp[recv_len], PACKET_SIZE); // db_pkt_t의 크기만큼 읽기
                if (recv_cnt == -1) 
                    error_handling("Receiving Packet Error!");

                recv_len += recv_cnt;
            }
            memcpy(&search_list, temp, sizeof(db_pkt_t));

            if (recv_len == -1) 
                error_handling("Failed to load search list");

            char db_data[BUF_SIZE];
            char cl_data[BUF_SIZE];
            strcpy(temp, search_list.search_word);
            strcpy(cl_data, query);
            strcpy(db_data, temp);
            toLowerCase(db_data);
            toLowerCase(cl_data);

            char *same = strstr(db_data, cl_data);
            if (same != NULL) 
            {
                int idx = same - db_data;
                printf("%.*s", idx, temp);
                printf(COLOR_ORANGE "%.*s" COLOR_RESET, (int)strlen(query), temp + idx);
                printf("%s\n", temp + idx + strlen(query));
            } 
            else 
                printf("%s\n", search_list.search_word);
        }
        
        printf("\033[%dA", count + 2);
        if(c != 127)
            printf("\033[%dC", ++to_right_cursor);
        else
            printf("\033[%dC", --to_right_cursor);
    }

    close(sock);
    return 0;
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

int getch()
{
	int c = 0;
	struct termios oldattr, newattr;

	tcgetattr(STDIN_FILENO, &oldattr);
	newattr = oldattr;
	newattr.c_lflag &= ~(ICANON | ECHO);
	newattr.c_cc[VMIN] = 1;
	newattr.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
	c = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);

	return c;
}

void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

void error_handling(char * message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(0);
}
