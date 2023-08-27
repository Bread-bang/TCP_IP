#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024

void error_handling(char * message);
void setup_sock(int * serv_sock, int port, struct sockaddr_in * serv_adr);

typedef struct
{
    int seq;
    char msg[BUF_SIZE];

}pkt_t;

int main(int argc, char * argv[])
{
    int serv_sock;
    int recv_ack;
    int str_len, tot_size = 0;
    socklen_t clnt_adr_sz;
    clock_t start, end;
    double tot_time;
    char message[BUF_SIZE];
    int msg_length;
    int err;

    struct sockaddr_in serv_adr, clnt_adr;
    pkt_t * send_pkt = (pkt_t*)malloc(sizeof(pkt_t));
    send_pkt->seq = 0;

    if(argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    setup_sock(&serv_sock, atoi(argv[1]), &serv_adr);

    clnt_adr_sz = sizeof(clnt_adr);
    recvfrom(serv_sock, &msg_length, sizeof(msg_length), 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
    recvfrom(serv_sock, message, msg_length, 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
    
    message[msg_length] = '\0';

    // Timeout setting
    struct timeval optVal = {1, 0};
    int optLen = sizeof(optVal);
    setsockopt(serv_sock, SOL_SOCKET, SO_RCVTIMEO, &optVal, optLen);

    FILE * fp;
    if((fp = fopen(message, "rb")) != NULL)
    {
        while(!feof(fp))
        {
            str_len = fread(send_pkt->msg, 1, BUF_SIZE - 1, fp);

            while(1)
            {
                sendto(serv_sock, send_pkt, sizeof(pkt_t), 0, (struct sockaddr*)&clnt_adr, clnt_adr_sz);
                recvfrom(serv_sock, &recv_ack, sizeof(recv_ack), 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
                if(recv_ack == send_pkt->seq)
                    break;
            }

            printf("[Packet %d] : succeed\n", recv_ack);
            if(recv_ack == 0)
                start = clock();
            tot_size += str_len;
            send_pkt->seq++;
        }
        send_pkt->seq = -1;
        sendto(serv_sock, send_pkt, sizeof(pkt_t), 0, (struct sockaddr*)&clnt_adr, clnt_adr_sz);
    }
    else
        error_handling("File open error!");
    
    end = clock();
    tot_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Throughput : %.2f KB/sec\n", (tot_size / 1024) / tot_time);

    fclose(fp);
    free(send_pkt);
    return 0;
}

void setup_sock(int * serv_sock, int port, struct sockaddr_in * serv_adr)
{
    *serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(*serv_sock == -1)
        error_handling("socket() error");
    
    memset(serv_adr, 0, sizeof(*serv_adr));
    serv_adr->sin_family = AF_INET;
    serv_adr->sin_addr.s_addr = htons(INADDR_ANY);
    serv_adr->sin_port = htons(port);

    if(bind(*serv_sock, (struct sockaddr*)serv_adr, sizeof(*serv_adr)) == -1)
        error_handling("bind() error");
}

void error_handling(char * message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
