#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define LINE 40
#define MAX_CLNT 256
#define QUERY_SIZE 100
#define BUF_SIZE 1024

typedef struct
{
    int whole_cnt;
    int search_time;
    char search_word[LINE];
} db_pkt_t;

int compare(const void * a, const void * b);
void make_db();
void setup_sock(int * serv_sock, int port, struct sockaddr_in * serv_adr);
void * communicate_clnt(void * arg);
void toLowerCase(char *str);
void error_handling(char * message);

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutex;
db_pkt_t * search_list = NULL;

int main(int argc, char * argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t t_id;
    search_list = (db_pkt_t*)malloc(sizeof(db_pkt_t));
    if(argc != 2)
    {
        printf("Usage : %s <port\n", argv[0]);
        exit(1);
    }

    // 파일 읽고 패킷에 저장
    make_db();

    // mutex 초기화
    pthread_mutex_init(&mutex, NULL);

    // 서버 세팅
    setup_sock(&serv_sock, atoi(argv[1]), &serv_adr);

    while(1)
    {
        // 클라이언트 연결
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_adr, &clnt_adr_sz);

        // 클라이언트 정보 업데이트
        pthread_mutex_lock(&mutex);
        clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutex);

        // 쓰레드 생성
        pthread_create(&t_id, NULL, communicate_clnt, (void*)&clnt_sock);
        pthread_detach(t_id);
        printf("Connected client IP : %s \n", inet_ntoa(clnt_adr.sin_addr));
    }
    close(serv_sock);
    return 0;
}

void make_db()
{
    FILE * fp;
    int count = 0, i;
    char line[LINE];

    if((fp = fopen("data.txt", "r")) == NULL)
        error_handling("File open error");

    while(fgets(line, LINE, fp) != NULL)
    {
        char * blank = strrchr(line, ' ');
        if(blank != NULL)
        {
            *blank = '\0';
            search_list[count].search_time = atoi(blank + 1);
            strcpy(search_list[count].search_word, line);

            count++;
            db_pkt_t * new_search_list = (db_pkt_t*)realloc(search_list, sizeof(db_pkt_t) * (count + 1));
            if(new_search_list == NULL)
                error_handling("Memory allocation failed");
            search_list = new_search_list;
        }
    }
    
    for(i = 0; i < count; i++)
        search_list[i].whole_cnt = count;

    qsort(search_list, count, sizeof(db_pkt_t), compare);

    // for(i = 0; i < count; i++)
    //     printf("%s %d %d\n", search_list[i].search_word, search_list[i].search_time, search_list[i].whole_cnt);

    fclose(fp);
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

int compare(const void * a, const void * b)
{
    const db_pkt_t * db_a = (const db_pkt_t *)a;
    const db_pkt_t * db_b = (const db_pkt_t *)b;

    return db_b->search_time - db_a->search_time;
}

void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

void * communicate_clnt(void * arg)
{
    int clnt_sock = *((int*)arg);
    int str_len, count = 0, i, list_cnt = 0;
    int recv_cnt, recv_len;
    char msg[QUERY_SIZE];
    char db_data[LINE];
    char cl_data[LINE];

    memset(msg, 0, sizeof(msg));

    while(1)
    {
        memset(db_data, 0, LINE);
        memset(cl_data, 0, LINE);
        
        recv_len = 0;
        while(recv_len < QUERY_SIZE)
        {
            recv_cnt = read(clnt_sock, &msg[recv_len], QUERY_SIZE - recv_len);
            if(recv_cnt == -1)
                error_handling("Reading msg error");
            recv_len += recv_cnt;
        }
        memcpy(cl_data, msg, sizeof(msg));
        toLowerCase(cl_data);

        if(strlen(cl_data) == 0)
        {
            list_cnt = 0;
            write(clnt_sock, &list_cnt, sizeof(int));
            continue;
        }

        for(i = 0; i < search_list[0].whole_cnt; i++)
        {
            memcpy(db_data, search_list[i].search_word, LINE);
            toLowerCase(db_data);
            
            if(strstr(db_data, cl_data) != NULL)
                list_cnt++;
        }
        write(clnt_sock, &list_cnt, sizeof(int));
        list_cnt = 0;
        printf("Search Result\n");
        printf("--------------\n");
        for(i = 0; i < search_list[0].whole_cnt; i++)
        {
            memcpy(db_data, search_list[i].search_word, LINE);
            toLowerCase(db_data);

            if(strstr(db_data, cl_data) != NULL)
            {
                str_len = write(clnt_sock, &search_list[i], sizeof(db_pkt_t));
                printf("%s\n", search_list[i].search_word);
            }
        }
        printf("\n");
    }

    // 클라이언트 remove
    pthread_mutex_lock(&mutex);
    for(i = 0; i < clnt_cnt; i++)
    {
        if(clnt_sock == clnt_socks[i])
        {
            while(i++ < clnt_cnt - 1)
                clnt_socks[i] = clnt_socks[i + 1];
            break;
        }
    }
    clnt_cnt--;
    pthread_mutex_unlock(&mutex);
    close(clnt_sock);
    return NULL;
}

void error_handling(char * message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(0);
}
