#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024

void error_handling(char * message);

typedef struct
{
    int seq;
    char msg[BUF_SIZE];
}pkt_t;

int main(int argc, char * argv[])
{
    int sock;
    int str_len, tot_size;
    int ack = 0;
    socklen_t adr_sz;
    char * filename = malloc(strlen(argv[3]) + 1);
    int filename_length;
    char recvmsg[BUF_SIZE];
    struct sockaddr_in serv_adr, from_adr;
    pkt_t * recv_pkt = (pkt_t*)malloc(sizeof(pkt_t));

    if(argc != 4)
    {
        printf("Usage : %s <IP> <PORT>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(sock == -1)
        error_handling("socket() error");
    
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_adr.sin_port = htons(atoi(argv[2]));

    strcpy(filename, argv[3]);
    filename_length = strlen(filename);

    adr_sz = sizeof(from_adr);
    sendto(sock, &filename_length, sizeof(filename_length), 0, (struct sockaddr*)&serv_adr, adr_sz);
    sendto(sock, filename, strlen(filename), 0, (struct sockaddr*)&serv_adr, adr_sz);

    FILE * fp;
    if((fp = fopen(filename, "wb")) != NULL)
    {
        int expected_seq = 0;
        while(1)
        {
            str_len = recvfrom(sock, recv_pkt, sizeof(pkt_t), 0, (struct sockaddr*)&from_adr, &adr_sz);
            if(str_len == -1)
                error_handling("파일 받기 실패");
                
            if(recv_pkt->seq == -1)
                break;

            if(recv_pkt->seq == expected_seq)
            {
                str_len = fwrite(recv_pkt->msg, 1, BUF_SIZE - 1, fp);
                expected_seq++;
            }
            
            sendto(sock, &recv_pkt->seq, sizeof(recv_pkt->seq), 0, (struct sockaddr*)&serv_adr, sizeof(serv_adr));
        }
    }
    else
        error_handling("File error !");
    
    free(recv_pkt);
    fclose(fp);
    return 0;
}

void error_handling(char * message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(0);
}
