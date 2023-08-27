#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAX_CLNT 20
#define FILE_NAME_STRLEN 100

typedef struct
{
    int id;
    int port;
    char ip_adr[INET_ADDRSTRLEN];
}adr_pkt_t;

typedef struct
{
    int file_size;
    int file_name_len;
    int sg_size;
    char file_name[FILE_NAME_STRLEN];
}file_head_pkt_t;

typedef struct
{
    int peace_of_file_idx;
    int this_size;
    char * content;
}file_body_pkt_t;

typedef struct
{
    int snd_sock;       // sending peer와 연결된 소켓
    int peer_tot_cnt;   // Receiving peer의 총 개수
    int conn_peer_cnt;  // 갖고 있는 Receiving 정보 개수(connect할 때 사용했던 packet_cnt)
    int conn_peer_sock;
    int * conn_peer_socks;
    int other_p_cnt;
    int other_p_sock;
    int * other_p_socks;
    file_head_pkt_t * file_head;
    file_body_pkt_t ** file_body; 
}arg_thread1_t;

typedef struct
{
    int sock;
    file_head_pkt_t * file_head;
    file_body_pkt_t ** file_body;
}arg_thread2_t;

void * handle_own_packet(void * arg);
void * handle_others_packet(void * arg);
void * handle_cursor(void * arg);

int who_am_i(int argc, char * argv[], int * max_num_recv_peer, int * segment_size, char * file_name, char * ip, char * listen_port, char * target_port);
void setup_sock(int * serv_sock, char * port, struct sockaddr_in * serv_adr);
void setup_clnt_socket(int * sock, char * ip, char * target_port, struct sockaddr_in * serv_adr);
void create_listening_sock(int * recv_sock, struct sockaddr_in * listen_adr, char * ip, char * listen_port);
void create_packets(int clnt_cnt, int * clnt_socks, adr_pkt_t * clnt_adr_info, struct sockaddr_in * clnt_adr);
void send_clnt_adr_info(adr_pkt_t * clnt_adr_info, int * clnt_socks, int clnt_cnt);
void connect_recv_peers(int * packet_cnt, int * conn_other_sock, int * conn_other_socks, adr_pkt_t * target_adr);
void accept_recv_peers(int accept_cnt, socklen_t * other_peer_adr_sz, struct sockaddr_in * other_peer_adr, struct sockaddr_in * other_peer_adrs, int * other_peer_sock, int * other_peer_socks, int * recv_sock);
void create_file_info(file_head_pkt_t * file_head_info_pkt, char * file_name, int segment_size);
void send_file_info(file_head_pkt_t * file_head_info_pkt, int clnt_cnt, int * clnt_socks);
void send_file_content(file_head_pkt_t * file_head_info_pkt, char * file_name, int segment_size, int clnt_cnt, int * clnt_socks);
int recv_file_segment(int send_sock, file_head_pkt_t * file_head, int * read_size, int * file_pkt_idx, char * content, int idx);
void send_file_segment(int conn_peer_cnt, int conn_peer_sock, int * conn_peer_socks, int other_p_cnt, int other_p_sock, int * other_p_socks, int * read_size, int * file_pkt_idx, char * content, file_head_pkt_t * file_head, int break_signal);
void build_threads(arg_thread1_t * info_spread);
void error_handling(char * message);

pthread_mutex_t mutex;

int main(int argc, char * argv[])
{
    // Option variables
    int server_or_client;
    int max_num_recv_peer = 0, segment_size = 0;
    char file_name[FILE_NAME_STRLEN], ip[INET_ADDRSTRLEN], listen_port[5], target_port[5];
    
    pthread_mutex_init(&mutex, NULL);
    
    // Option 정보 저장
    server_or_client = who_am_i(argc, argv, &max_num_recv_peer, &segment_size, file_name, ip, listen_port, target_port);
    // Sending peer
    if(server_or_client == 's')
    {
        int i, j;
        int serv_sock;
        int clnt_sock, clnt_cnt = 0, clnt_socks[max_num_recv_peer];
        struct sockaddr_in serv_adr, clnt_adr[max_num_recv_peer];
        socklen_t clnt_adr_sz;
        adr_pkt_t * clnt_adr_info = (adr_pkt_t*)malloc(sizeof(adr_pkt_t) * max_num_recv_peer);

        // 소켓 세팅
        setup_sock(&serv_sock, listen_port, &serv_adr);

        // 클라이언트 연결
        while(max_num_recv_peer > clnt_cnt)
        {
            clnt_adr_sz = sizeof(clnt_adr);
            clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_adr[clnt_cnt], &clnt_adr_sz);
            clnt_socks[clnt_cnt++] = clnt_sock;
            printf("Client connected : %d\n", clnt_sock);
        }

        for(i = 0; i < clnt_cnt; i++)
            write(clnt_socks[i], &clnt_cnt, sizeof(int));

        // Port 정보 받고 패킷에 저장
        create_packets(clnt_cnt, clnt_socks, clnt_adr_info, clnt_adr);

        // Receiving Peer들에게 클라이언트 패킷 정보 뿌려주기
        send_clnt_adr_info(clnt_adr_info, clnt_socks, clnt_cnt);

        // init 확인하기
        int ready[clnt_cnt];
        for(i = 0; i < clnt_cnt; i++)
            read(clnt_socks[i], &ready[i], sizeof(int));
        
        printf("\n===== All clients are ready! =====\n\n");

        // 파일 정보
        file_head_pkt_t * file_head_info_pkt = (file_head_pkt_t*)malloc(sizeof(file_head_pkt_t));
        create_file_info(file_head_info_pkt, file_name, segment_size);           // 만들기
        send_file_info(file_head_info_pkt, clnt_cnt, clnt_socks);                // 보내기

        // 파일 내용 보내기
        send_file_content(file_head_info_pkt, file_name, segment_size, clnt_cnt, clnt_socks);
    }
    // Receiving peer
    else if(server_or_client == 'r')
    {
        int i, peer_cnt, packet_cnt;
        int send_sock, recv_sock, str_len;
        struct sockaddr_in serv_adr, listen_adr;

        // 클라이언트 소켓 세팅 및 Sending peer 연결
        setup_clnt_socket(&send_sock, ip, target_port, &serv_adr);

        // listening socket 세팅
        create_listening_sock(&recv_sock, &listen_adr, ip, listen_port);
        
        // 총 peer 개수 받아오기
        read(send_sock, &peer_cnt, sizeof(int));

        // Sending peer에게 각자의 Port 정보 전송
        str_len = strlen(listen_port);
        write(send_sock, &str_len, sizeof(int));
        write(send_sock, listen_port, strlen(listen_port));
        
        // 패킷 개수 받기
        read(send_sock, &packet_cnt, sizeof(int));

        // connect할 Receiving Peer들 정보 가져오기
        adr_pkt_t * target_adr = (adr_pkt_t*)malloc(sizeof(adr_pkt_t) * packet_cnt);
        for(i = 0; i < packet_cnt; i++)
            read(send_sock, &target_adr[i], sizeof(adr_pkt_t));
        
        // connect
        int conn_other_sock, conn_other_socks[packet_cnt];                
        connect_recv_peers(&packet_cnt, &conn_other_sock, conn_other_socks, target_adr);
        
        // accept
        int accept_cnt = peer_cnt - (packet_cnt + 1);                   // accept 횟수
        int other_peer_sock, other_peer_socks[accept_cnt];              // Receiving peer를 의미하는 clnt_sock
        struct sockaddr_in other_peer_adr, other_peer_adrs[accept_cnt]; // Receiving peer의 주소
        socklen_t other_peer_adr_sz;
        accept_recv_peers(accept_cnt, &other_peer_adr_sz, &other_peer_adr, other_peer_adrs, &other_peer_sock, other_peer_socks, &recv_sock);
        
        // init 보내기
        int is_ready = 1;
        write(send_sock, &is_ready, sizeof(int));

        // 파일 정보 받기
        file_head_pkt_t * file_head_info_pkt = (file_head_pkt_t*)malloc(sizeof(file_head_pkt_t));
        read(send_sock, file_head_info_pkt, sizeof(file_head_pkt_t));

        // 파일 이름 받기
        char file_name[FILE_NAME_STRLEN];
        strncpy(file_name, file_head_info_pkt->file_name, file_head_info_pkt->file_name_len);

        // 2차원 배열 크기 구하기
        int arr_size = file_head_info_pkt->file_size / file_head_info_pkt->sg_size;
        if(file_head_info_pkt->file_size % file_head_info_pkt->sg_size != 0)
            arr_size += 1;

        // 공간 만들기
        file_body_pkt_t ** file_body_info_pkt = (file_body_pkt_t**)malloc((arr_size + 1)* sizeof(file_body_pkt_t*));
        for (i = 0; i < arr_size; i++) 
        {
            file_body_info_pkt[i] = (file_body_pkt_t*)malloc(sizeof(file_body_pkt_t));
            if (file_body_info_pkt[i] == NULL) {
                perror("Failed to allocate memory for file_body_info_pkt[i]");
                exit(1);
            }
            
            file_body_info_pkt[i]->peace_of_file_idx = 0;
            file_body_info_pkt[i]->this_size = 0;
            file_body_info_pkt[i]->content = (char*)malloc(file_head_info_pkt->sg_size);
            if (file_body_info_pkt[i]->content == NULL) {
                perror("Failed to allocate memory for file_body_info_pkt[i]->content");
                exit(1);
            }
            memset(file_body_info_pkt[i]->content, 0, file_head_info_pkt->sg_size);
        }

        // 쓰레드 구조체 인자 만들기
        arg_thread1_t * info_spread = (arg_thread1_t*)malloc(sizeof(arg_thread1_t));
        info_spread->snd_sock = send_sock;
        info_spread->peer_tot_cnt = peer_cnt;
        info_spread->conn_peer_cnt = packet_cnt;
        info_spread->conn_peer_sock = conn_other_sock;
        info_spread->conn_peer_socks = (int*)malloc(sizeof(int) * packet_cnt);
            memcpy(info_spread->conn_peer_socks, conn_other_socks, sizeof(int) * packet_cnt);
        info_spread->other_p_cnt = accept_cnt;
        info_spread->other_p_sock = other_peer_sock;
        info_spread->other_p_socks = (int*)malloc(sizeof(int) * accept_cnt);
            memcpy(info_spread->other_p_socks, other_peer_socks, sizeof(int) * accept_cnt);
        info_spread->file_head = file_head_info_pkt;
        info_spread->file_body = file_body_info_pkt;

        // Sending peer -> Me -> Others
        pthread_t from_sender_thread;
        pthread_create(&from_sender_thread, NULL, handle_own_packet, (void*)info_spread);

        // Others -> Me
        build_threads(info_spread);

        pthread_join(from_sender_thread, NULL);

        FILE * fp;
        if((fp = fopen(file_name, "wb")) != NULL)
        {
            for(i = 0; i < arr_size; i++)
                fwrite(file_body_info_pkt[i]->content, 1, file_body_info_pkt[i]->this_size, fp);
        }
        fclose(fp);

        for(i = 0; i < arr_size; i++)
        {
            free(file_body_info_pkt[i]->content);
            free(file_body_info_pkt[i]);
        }
        free(file_body_info_pkt);
    }
    pthread_mutex_destroy(&mutex);
    return 0;
}

int who_am_i(int argc, char * argv[], int * max_num_recv_peer, int * segment_size, char * file_name, char * ip, char * listen_port, char * target_port)
{
    int server_or_client;
    int opt;

    while((opt = getopt(argc, argv, "srn:f:g:a:p:")) != -1)
    {
        switch(opt)
        {
            case 's':
                server_or_client = 's';
                break;
            case 'r':
                server_or_client = 'r';
                break;
            case 'n':
                *max_num_recv_peer = atoi(optarg);
                break;
            case 'f':
                strncpy(file_name, optarg, strlen(optarg));
                file_name[strlen(optarg)] = '\0';
                break;
            case 'g':
                *segment_size = atoi(optarg) * 1024;
                break;
            case 'a':
                strncpy(ip, optarg, INET_ADDRSTRLEN - 1);
                ip[INET_ADDRSTRLEN - 1] = '\0';
                strncpy(target_port, argv[optind++], 4);
                target_port[4] = '\0';
                break;
            case 'p':
                strncpy(listen_port, optarg, 4);
                listen_port[4] = '\0';
                break;
            default:
                fprintf(stderr, "Unknown option: -%c\n", optopt);
                exit(EXIT_FAILURE);
                break;
        }
    }
    return server_or_client;
}

void setup_sock(int * serv_sock, char * listen_port, struct sockaddr_in * serv_adr)
{
    *serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(*serv_sock == -1)
            error_handling("socket() error");

    memset(serv_adr, 0, sizeof(*serv_adr));
    serv_adr->sin_family = AF_INET;
    serv_adr->sin_addr.s_addr = htons(INADDR_ANY);
    serv_adr->sin_port = htons(atoi(listen_port));

    int reuse = 1;
    setsockopt(*serv_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int buff_size = 5 * 1024 * 1024;
    if (setsockopt(*serv_sock, SOL_SOCKET, SO_SNDBUF, &buff_size, sizeof(buff_size)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if(bind(*serv_sock, (struct sockaddr*)serv_adr, sizeof(*serv_adr)) == -1)
            error_handling("bind() error");

    if(listen(*serv_sock, MAX_CLNT) == -1)
            error_handling("listen() error");
}

void setup_clnt_socket(int * sock, char * ip, char * target_port, struct sockaddr_in * serv_adr)
{
    *sock = socket(PF_INET, SOCK_STREAM, 0);
    if(*sock == -1)
        error_handling("socket() error");

    memset(serv_adr, 0, sizeof(*serv_adr));
    serv_adr->sin_family = AF_INET;
    serv_adr->sin_addr.s_addr = inet_addr(ip);
    serv_adr->sin_port = htons(atoi(target_port));

    if(connect(*sock, (struct sockaddr*)serv_adr, sizeof(*serv_adr)) == -1)
        error_handling("connect() error");
    printf("Sending peer 연결 완료\n");
}

void create_listening_sock(int * recv_sock, struct sockaddr_in * listen_adr, char * ip, char * listen_port)
{
    *recv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(listen_adr, 0, sizeof(*listen_adr));
    listen_adr->sin_family = AF_INET;
    listen_adr->sin_addr.s_addr = htons(INADDR_ANY);
    listen_adr->sin_port = htons(atoi(listen_port));
    
    int reuse = 1;
    setsockopt(*recv_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int buff_size = 5 * 1024 * 1024;
    if (setsockopt(*recv_sock, SOL_SOCKET, SO_SNDBUF, &buff_size, sizeof(buff_size)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if(bind(*recv_sock, (struct sockaddr*)listen_adr, sizeof(*listen_adr)) == -1)
        perror("bind() error in Receiving peer");
    
    if(listen(*recv_sock, MAX_CLNT) == -1)
        error_handling("listen() error in Receiving peer");
}

void create_packets(int clnt_cnt, int * clnt_socks, adr_pkt_t * clnt_adr_info, struct sockaddr_in * clnt_adr)
{
    int i;
    char recv_peer_listen_port[clnt_cnt][5];

    for(i = 0; i < clnt_cnt; i++)
    {
        int str_len;
        // Receiving peer로부터 port 받기
        read(clnt_socks[i], &str_len, sizeof(int));
        read(clnt_socks[i], recv_peer_listen_port[i], str_len);
        recv_peer_listen_port[i][str_len] = '\0';

        // 패킷에 ID 저장
        clnt_adr_info[i].id = i;

        // 패킷에 port 저장
        clnt_adr_info[i].port = atoi(recv_peer_listen_port[i]);

        // Receiving peer의 IP
        strcpy(clnt_adr_info[i].ip_adr, inet_ntoa(clnt_adr[i].sin_addr));
    }
}

void send_clnt_adr_info(adr_pkt_t * clnt_adr_info, int * clnt_socks, int clnt_cnt)
{
    int i, j, cnt;

    // 패킷 개수 뿌리기
    for(i = 0; i < clnt_cnt; i++)
    {
        cnt = 0;
        for(j = i + 1; j < clnt_cnt; j++)
            cnt++;
        write(clnt_socks[i], &cnt, sizeof(int));
    }

    // 패킷 뿌리기
    for(i = 0; i < clnt_cnt; i++)
    {
        for(j = i + 1; j < clnt_cnt; j++)
            write(clnt_socks[i], &clnt_adr_info[j], sizeof(adr_pkt_t));
    }
}

void connect_recv_peers(int * packet_cnt, int * conn_other_sock, int * conn_other_socks, adr_pkt_t * target_adr)
{
    int i;
    if(*packet_cnt > 0)
    {
        if(*packet_cnt == 1)
        {
            *conn_other_sock = socket(PF_INET, SOCK_STREAM, 0);
            if(*conn_other_sock == -1)
                error_handling("socket() error while connecting others");

            struct sockaddr_in other_recv_peer_adr;
            memset(&other_recv_peer_adr, 0, sizeof(other_recv_peer_adr));
            other_recv_peer_adr.sin_family = AF_INET;
            other_recv_peer_adr.sin_addr.s_addr = inet_addr(target_adr[0].ip_adr);
            other_recv_peer_adr.sin_port = htons(target_adr[0].port);

            if(connect(*conn_other_sock, (struct sockaddr*)&other_recv_peer_adr, sizeof(other_recv_peer_adr)) == -1)
                perror("connect_recv_peers() error\n");
            else
                printf("Succeed to connect : %d\n", *conn_other_sock);
        }
        else
        {
            conn_other_socks[*packet_cnt];
            struct sockaddr_in other_recv_peer_adr[*packet_cnt];
            for(i = 0; i < *packet_cnt; i++)
            {
                conn_other_socks[i] = socket(PF_INET, SOCK_STREAM, 0);
                if(conn_other_socks[i] == -1)
                    error_handling("socket() error while connecting others[many]");

                memset(&other_recv_peer_adr[i], 0, sizeof(other_recv_peer_adr[i]));
                other_recv_peer_adr[i].sin_family = AF_INET;
                other_recv_peer_adr[i].sin_addr.s_addr = inet_addr(target_adr[i].ip_adr);
                other_recv_peer_adr[i].sin_port = htons(target_adr[i].port);

                if(connect(conn_other_socks[i], (struct sockaddr*)&other_recv_peer_adr[i], sizeof(other_recv_peer_adr[i])) == -1)
                    perror("connect_others() error\n");
                else
                    printf("Succeed to connect : %d\n", conn_other_socks[i]);
            }
        }
    }
}

void accept_recv_peers(int accept_cnt, socklen_t * other_peer_adr_sz, struct sockaddr_in * other_peer_adr, struct sockaddr_in * other_peer_adrs, int * other_peer_sock, int * other_peer_socks, int * recv_sock)
{
    int i;
    if(accept_cnt > 0)
    {
        if(accept_cnt == 1)
        {
            *other_peer_adr_sz = sizeof(*other_peer_adr);
            *other_peer_sock = accept(*recv_sock, (struct sockaddr*) other_peer_adr, other_peer_adr_sz);
            printf("Succeed to accept : %d\n", *other_peer_sock);
        }
        else
        {
            for(i = 0; i < accept_cnt; i++)
            {
                *other_peer_adr_sz = sizeof(other_peer_adrs[i]);
                other_peer_sock[i] = accept(*recv_sock, (struct sockaddr*) &other_peer_adrs[i], other_peer_adr_sz);
                other_peer_socks[i] = other_peer_sock[i];
                printf("Succeed to accept : %d\n", other_peer_sock[i]);
            }
        }
    }
}

void create_file_info(file_head_pkt_t * file_head_info_pkt, char * file_name, int segment_size)
{
    struct stat sb;
    if(stat(file_name, &sb) == -1)
        perror("Failed to get file info");

    file_head_info_pkt->file_size = sb.st_size;
    file_head_info_pkt->file_name_len = strlen(file_name);
    file_head_info_pkt->sg_size = segment_size;
    strcpy(file_head_info_pkt->file_name, file_name);
}

void send_file_info(file_head_pkt_t * file_head_info_pkt, int clnt_cnt, int * clnt_socks)
{
    for(int i = 0; i < clnt_cnt; i++)
        write(clnt_socks[i], file_head_info_pkt, sizeof(file_head_pkt_t));
}

void send_file_content(file_head_pkt_t * file_head_info_pkt, char * file_name, int segment_size, int clnt_cnt, int * clnt_socks)
{
    FILE * fp;
    int i, j;
    int read_size;
    char content[segment_size];

    if((fp = fopen(file_name, "rb")) != NULL)
    {
        int sub_tot_clnt[clnt_cnt];
        memset(sub_tot_clnt, 0, sizeof(sub_tot_clnt));
        int cur_len = 0, clnt_idx = 0;
        int file_pkt_idx = 0;

        printf("Sending Peer  ");
        printf("[");
        for(i = 0; i < (cur_len * 50 / file_head_info_pkt->file_size) / 2; i++)
                printf("#");
        for(j = i; j < 50 / 2; j++)
                printf(" ");
        printf("]");
        printf(" %d%%", (cur_len * 100 / file_head_info_pkt->file_size));
        printf(" (%d / %dBytes)\n", cur_len, file_head_info_pkt->file_size);
        for(i = 0; i < clnt_cnt; i++)
            printf(" To Receiving Peer #%d : (%d Bytes Sent) \n", (i + 1), sub_tot_clnt[i]);
        usleep(5000);
        for(i = 0; i < clnt_cnt + 1; i++)
        {
            printf("\033[A\033[k");
            usleep(5000);
        }
        usleep(5000);
        while(1)
        {
            read_size = fread(content, 1, segment_size, fp);
            write(clnt_socks[clnt_idx], &read_size, sizeof(int));
            write(clnt_socks[clnt_idx], &file_pkt_idx, sizeof(int));
            write(clnt_socks[clnt_idx], content, segment_size);
            cur_len += read_size;
            sub_tot_clnt[clnt_idx] += read_size;

            printf("Sending Peer  ");
            printf("[");
            for(i = 0; i < (cur_len * 50 / file_head_info_pkt->file_size) / 2; i++)
                printf("#");
            for(j = i; j < 50 / 2; j++)
                printf(" ");
            printf("]");
            printf(" %d%%", (cur_len * 100 / file_head_info_pkt->file_size));
            printf(" (%d / %dBytes)\n", cur_len, file_head_info_pkt->file_size);
            for(i = 0; i < clnt_cnt; i++)
                printf(" To Receiving Peer #%d : (%d Bytes Sent) \n", (i + 1), sub_tot_clnt[i]);
            usleep(5000);
            file_pkt_idx++;
            clnt_idx = (clnt_idx + 1) % clnt_cnt;
            if(cur_len >= file_head_info_pkt->file_size)
                break;
            else
            {
                usleep(5000);
                for(i = 0; i < clnt_cnt + 1; i++)
                    printf("\033[A\033[k");
                usleep(5000);
            }
        }
        int end_signal = -1;
        for(int i = 0; i < clnt_cnt; i++)
            write(clnt_socks[i], &end_signal, sizeof(int));
    }
    else
        perror("File open error");
    fclose(fp);
}

void * handle_own_packet(void * arg)
{
    arg_thread1_t * info = (arg_thread1_t *) arg;
    int snd_sock = info->snd_sock;
    int peer_tot_cnt = info->peer_tot_cnt;
    int conn_peer_cnt = info->conn_peer_cnt;
    int conn_peer_sock = info->conn_peer_sock;
    int conn_peer_socks[conn_peer_cnt];
        memcpy(conn_peer_socks, info->conn_peer_socks, sizeof(int) * conn_peer_cnt);
    int other_p_cnt = info->other_p_cnt;
    int other_p_sock = info->other_p_sock;
    int other_p_socks[other_p_cnt];
        memcpy(other_p_socks, info->other_p_socks, sizeof(int) * other_p_cnt);
    file_head_pkt_t * file_head = info->file_head;
    file_body_pkt_t ** file_body = info->file_body;
    int break_signal = 0;
    int total = 0;
    int i = 0;
    int read_size, file_pkt_idx;
    char content[file_head->sg_size];
    while(1)
    {
        // segment 단위로 받아와서 file_segment에 저장
        break_signal = recv_file_segment(snd_sock, file_head, &read_size, &file_pkt_idx, content, i);
        
        // spread segment to other peers
        send_file_segment(conn_peer_cnt, conn_peer_sock, conn_peer_socks, other_p_cnt, other_p_sock, other_p_socks, &read_size, &file_pkt_idx, content, file_head, break_signal);

        if(break_signal == 1)
            break;

        // // file_body에 저장
        file_body[file_pkt_idx]->peace_of_file_idx = file_pkt_idx;
        file_body[file_pkt_idx]->this_size = read_size;
        memcpy(file_body[file_pkt_idx]->content, content, read_size);

        total += read_size;
        i++;
    }
    printf("total : %d\n", total);
}

int recv_file_segment(int send_sock, file_head_pkt_t * file_head, int * read_size, int * file_pkt_idx, char * content, int idx)
{
    int i, j;
    int cur_len = 0;
    int recv_cnt, recv_len;
    read(send_sock, read_size, sizeof(int));
    if(*read_size == -1)
        return 1;

    read(send_sock, file_pkt_idx, sizeof(int));
    recv_len = 0;
    while(recv_len < file_head->sg_size)
    {
        recv_cnt = read(send_sock, &content[recv_len], file_head->sg_size - recv_len);
        recv_len += recv_cnt;
    }
    cur_len += *read_size;
    return 0;
}

void send_file_segment(int conn_peer_cnt, int conn_peer_sock, int * conn_peer_socks, int other_p_cnt, int other_p_sock, int * other_p_socks, int * read_size, int * file_pkt_idx, char * content, file_head_pkt_t * file_head, int break_signal)
{
    int i;
    int sock_cnt = 0;
    int send_size = *read_size;

    if(break_signal == 1)
    {
        *read_size = -1;
        *file_pkt_idx = -1;
        memset(content, 0, file_head->sg_size);
    }   

    if(conn_peer_cnt > 0)
    {
        if(conn_peer_cnt == 1)
        {
            write(conn_peer_sock, read_size, sizeof(int));
            write(conn_peer_sock, file_pkt_idx, sizeof(int));
            write(conn_peer_sock, content, send_size);
        }
        else
        {
            for(i = 0; i < conn_peer_cnt; i++)
            {
                write(conn_peer_socks[i], read_size, sizeof(int));
                write(conn_peer_socks[i], file_pkt_idx, sizeof(int));
                write(conn_peer_socks[i], content, send_size);
            }
        }
    }
    if(other_p_cnt > 0)
    {
        if(other_p_cnt == 1)
        {
            write(other_p_sock, read_size, sizeof(int));
            write(other_p_sock, file_pkt_idx, sizeof(int));
            write(other_p_sock, content, send_size);
        } 
        else
        {
            for(i = 0; i < other_p_cnt; i++)
            {
                write(other_p_socks[i], read_size, sizeof(int));
                write(other_p_socks[i], file_pkt_idx, sizeof(int));
                write(other_p_socks[i], content, send_size);
            }
        }
    }
}

void build_threads(arg_thread1_t * info_spread)
{
    int i;
    int conn_p_size = info_spread->conn_peer_cnt;
    int other_p_size = info_spread->other_p_cnt;

    arg_thread2_t * conn_arg_recv;
    pthread_t conn_recv_thread;
    arg_thread2_t * conn_arg_recv_array[conn_p_size];
    pthread_t conn_recv_thread_mult[conn_p_size];

    arg_thread2_t * other_arg_recv;
    pthread_t other_recv_thread;
    arg_thread2_t * other_arg_recv_array[other_p_size];
    pthread_t other_recv_thread_mult[other_p_size];

    if(info_spread->conn_peer_cnt > 0)
    {
        if(info_spread->conn_peer_cnt == 1)
        {
            conn_arg_recv = (arg_thread2_t*)malloc(sizeof(arg_thread2_t));
            conn_arg_recv->file_head = info_spread->file_head;
            conn_arg_recv->file_body = info_spread->file_body;
            conn_arg_recv->sock = info_spread->conn_peer_sock;
            pthread_create(&conn_recv_thread, NULL, handle_others_packet, (void*)conn_arg_recv);
        }
        else
        {
            for(i = 0; i < conn_p_size; i++)
            {
                conn_arg_recv_array[i] = (arg_thread2_t*)malloc(sizeof(arg_thread2_t));
                conn_arg_recv_array[i]->file_head = info_spread->file_head;
                conn_arg_recv_array[i]->file_body = info_spread->file_body;
                conn_arg_recv_array[i]->sock = info_spread->conn_peer_socks[i];
                pthread_create(&conn_recv_thread_mult[i], NULL, handle_others_packet, (void*)conn_arg_recv_array[i]);
            }
        }
    }
    if(info_spread->other_p_cnt > 0)
    {
        if(info_spread->other_p_cnt == 1)
        {
            other_arg_recv = (arg_thread2_t*)malloc(sizeof(arg_thread2_t));
            other_arg_recv->file_head = info_spread->file_head;
            other_arg_recv->file_body = info_spread->file_body;
            other_arg_recv->sock = info_spread->other_p_sock;
            pthread_create(&other_recv_thread, NULL, handle_others_packet, (void*)other_arg_recv);
        }
        else
        {
            for(i = 0; i < other_p_size; i++)
            {
                other_arg_recv_array[i] = (arg_thread2_t*)malloc(sizeof(arg_thread2_t));
                other_arg_recv_array[i]->file_head = info_spread->file_head;
                other_arg_recv_array[i]->file_body = info_spread->file_body;
                other_arg_recv_array[i]->sock = info_spread->other_p_socks[i];
                pthread_create(&other_recv_thread_mult[i], NULL, handle_others_packet, (void*)other_arg_recv_array[i]);
            }
        }
    }

    if(info_spread->conn_peer_cnt == 1)
        pthread_join(conn_recv_thread, NULL);
    else
    {
        for(i = 0; i < conn_p_size; i++)
            pthread_join(conn_recv_thread_mult[i], NULL);
    }

    if(info_spread->other_p_cnt == 1)
        pthread_join(other_recv_thread, NULL);
    else
    {
        for(i = 0; i < other_p_size; i++)
            pthread_join(other_recv_thread_mult[i], NULL);
    }
}

void * handle_others_packet(void * arg)
{
    arg_thread2_t * info = (arg_thread2_t *) arg;
    file_body_pkt_t * file_segment;
    int sock = info->sock;
    int recv_len, recv_cnt;
    int read_size;
    int file_pkt_idx;
    char content[info->file_head->sg_size];
    
    

void error_handling(char * message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(0);
}
