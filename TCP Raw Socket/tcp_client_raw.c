#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

# define TH_FIN  0x01
# define TH_SYN  0x02
# define TH_RST  0x04
# define TH_PUSH 0x08
# define TH_ACK  0x10
# define TH_URG  0x20

#define INITIAL_VALUE 0
#define NO_PAYLOAD 0
#define PACKET_LEN 4096
#define BUF_SIZE 1024

// Before run this code, execute the command below 
// $ sudo iptables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP

// TODO: pseudo header needed for tcp header checksum calculation
// 소스 IP, 대상 IP, 예약 필드, 프로토콜 번호(TCP는 6), TCP 세그먼트의 길이
struct pseudo_header
{
    uint32_t src_addr_ip;
    uint32_t dest_addr_ip;
    uint8_t placeholder;
    uint8_t protocol;
    uint16_t tcp_seg_length;
};

unsigned short checksum(const char * buf, int len);
void recv_ack(int sock, char * buffer, unsigned short * dst_port, struct sockaddr_in * saddr);
void switch_seq_ack(char * buffer, uint32_t * previous_seq, uint32_t * previous_ack_num);
void create_ip_header(struct iphdr * ip_header, uint16_t * packet_id, struct sockaddr_in * saddr, struct sockaddr_in * daddr, int payload_len);
void create_pkt(char * buffer, int * pkt_len, struct sockaddr_in * saddr, struct sockaddr_in * daddr, struct iphdr * ip_header, struct tcphdr * tcp_header, uint16_t * packet_id, int flags, uint32_t * previous_seq, uint32_t * ack_num, char * payload, int payload_len);

// TODO 
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s <Source IP> <Destination IP> <Destination Port>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Source IP
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(rand() % 65535); // random client port
    if (inet_pton(AF_INET, argv[1], &saddr.sin_addr) != 1)
    {
        perror("Source IP configuration failed\n");
        exit(EXIT_FAILURE);
    }

    // Destination IP and Port 
    struct sockaddr_in daddr;
    daddr.sin_family = AF_INET;
    daddr.sin_port = htons(atoi(argv[3]));
    if (inet_pton(AF_INET, argv[2], &daddr.sin_addr) != 1)
    {
        perror("Destination IP and Port configuration failed");
        exit(EXIT_FAILURE);
    }

    // Tell the kernel that headers are included in the packet
    int one = 1;
    const int *val = &one;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) == -1)
    {
        perror("setsockopt(IP_HDRINCL, 1)");
        exit(EXIT_FAILURE);
    }
    uint16_t packet_id = 0;
    int pkt_len;
    char message[BUF_SIZE];

    char * buffer = malloc(PACKET_LEN);
    struct iphdr * ip_header = (struct iphdr*)buffer;
    struct tcphdr * tcp_header = (struct tcphdr*)(buffer + sizeof(struct iphdr)); // IP_header should be followed by TCP_header.

    // TCP Three-way Handshaking 
    // Step 1. Send SYN (no need to use TCP options) 
    create_pkt(buffer, &pkt_len, &saddr, &daddr, ip_header, tcp_header, &packet_id, TH_SYN, INITIAL_VALUE, INITIAL_VALUE, message, NO_PAYLOAD);

    int send_result;
    if((send_result = sendto(sock, buffer, pkt_len, 0, (struct sockaddr*)&daddr, sizeof(struct sockaddr))) == -1)
    {   
        perror("Error : sendto() is failed");
        exit(EXIT_FAILURE);
    }

    // Step 2. Receive SYN-ACK
    unsigned short dst_port;
    memset(buffer, 0, PACKET_LEN);
    recv_ack(sock, buffer, &dst_port, &saddr);

    // Step 3. Send ACK 
    uint32_t previous_seq = 0, previous_ack_num = 0;
    switch_seq_ack(buffer, &previous_seq, &previous_ack_num);

    create_pkt(buffer, &pkt_len, &saddr, &daddr, ip_header, tcp_header, &packet_id, TH_ACK, &previous_seq, &previous_ack_num, message, NO_PAYLOAD);
    if((send_result = sendto(sock, buffer, pkt_len, 0, (struct sockaddr*)&daddr, sizeof(struct sockaddr))) == -1)
    {   
        perror("Error : sendto() is failed");
        exit(EXIT_FAILURE);
    }

    // Data transfer 
    int payload_len;
    while (1) 
    {
        fputs("Input message(Q to quit): ", stdout);
        fgets(message, BUF_SIZE, stdin);
        
        if (!strcmp(message,"q\n") || !strcmp(message,"Q\n"))
            break;

        // Step 4. Send an application message (with PSH and ACK flag)!
        payload_len = strlen(message);
        create_pkt(buffer, &pkt_len, &saddr, &daddr, ip_header, tcp_header, &packet_id, TH_PUSH + TH_ACK, &previous_seq, &previous_ack_num, message, payload_len);
        if((send_result = sendto(sock, buffer, pkt_len, 0, (struct sockaddr*)&daddr, sizeof(struct sockaddr))) == -1)
        {   
            perror("Error : sendto() is failed");
            exit(EXIT_FAILURE);
        }

        // Step 5. Receive ACK
        memset(buffer, 0, PACKET_LEN);
        dst_port = 0;
        recv_ack(sock, buffer, &dst_port, &saddr);
        switch_seq_ack(buffer, &previous_seq, &previous_ack_num);
    }

    free(buffer);
    close(sock);
    return 0;
}

// TODO: Define checksum function which returns unsigned short value 
// IP  : IP 헤더의 모든 16비트 워드의 합
// TCP : ‘pseudo header’, TCP 헤더, TCP 데이터의 모든 16비트 워드의 합
unsigned short checksum(const char * buf, int len)
{
    unsigned int sum = 0;
    unsigned short * bytes_16 = (unsigned short *) buf;
    while(len > 1)
    {
        sum += *(bytes_16)++;
        if(sum & 0x80000000) // 오버플로 처리
            sum = (sum & 0xFFFF) + (sum >> 16);
        len -= 2;
    }

    // if there is smaller data than 2bytes, it should be calculated.
    if(len)
        sum += *(unsigned char *)bytes_16;
    
    // Handle the overflow
    while(sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    
    return (unsigned short)(~sum);
}

void recv_ack(int sock, char * buffer, unsigned short * dst_port, struct sockaddr_in * saddr)
{
    // Accept all the packet and compare the port whether it is proper port.
    *dst_port = 0;
    int recv_result = 0;
    while (*dst_port != saddr->sin_port){
        recv_result = recvfrom(sock, buffer, PACKET_LEN, 0, NULL, NULL); // src_addr가 NULL인 이유는 Raw socket을 통해 모든 패킷을 핸들링 하기 위해서.
        if(recv_result < 0)
            break;
        memcpy(dst_port, buffer + 22, sizeof(unsigned short));
    }
}

void switch_seq_ack(char * buffer, uint32_t * previous_seq, uint32_t * previous_ack_num)
{
    memcpy(previous_seq, buffer + 24, sizeof(uint32_t));
    memcpy(previous_ack_num, buffer + 28, sizeof(uint32_t));

    *previous_seq = ntohl(*previous_seq);
    *previous_ack_num = ntohl(*previous_ack_num);
}

void create_ip_header(struct iphdr * ip_header, uint16_t * packet_id, struct sockaddr_in * saddr, struct sockaddr_in * daddr, int payload_len)
{
    ip_header->version = 4;     // IPv4 : 4, IPv6 : 6
    ip_header->ihl = 5;         // Length of Internet protocol header.
    ip_header->tos = 0;         // Upper 6 bits of tos is about DSCP which is related to data's priority and Quality of Service.
                                // Lower 2 bits of tos is about ECN.
    ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + payload_len;  // Options may not be neeeded because the packet will be just SYN packet.
    ip_header->id = htons((*packet_id)++);                                // When IP fragmentation occurs, this is used to identify that different fragments are part of the same original packet.
                                                                        // This is manual way.
    ip_header->frag_off = htons(1 << 14);       // This is used to figure out the relative location of this fragment in original packet.
                                                // In case of SYN packet, since the size of the packet is small, there is no reason to use this field.
    ip_header->ttl = 255;                       // the number of maximum hops. The ttl field has 1byte. So the maximum value is 255.
    ip_header->protocol = IPPROTO_TCP;          // The protocol type of upper layer.
    ip_header->check = 0;                       // This is used to examine the integrity of the packet. The standard of checksum is introduced in RFC 1071.
                                                // In the initial stage, the value should be 0. Then, calculate it later.
    ip_header->saddr = saddr->sin_addr.s_addr;
    ip_header->daddr = daddr->sin_addr.s_addr;
}

void create_tcp_header(struct tcphdr * tcp_header, struct sockaddr_in * saddr, struct sockaddr_in * daddr, int flags, uint32_t * previous_seq, uint32_t * ack_num)
{
    // printf("[create_tcp_header] previous_seq : %u ack_num : %u\n", previous_seq, ack_num);
    tcp_header->source = saddr->sin_port;
    tcp_header->dest = daddr->sin_port;
    if(flags == TH_SYN)
    {
        tcp_header->seq = htonl(rand() * 4294967295); // Seq should be randomized. The maximum number of 4bytes is represented by 4294967295;
        tcp_header->ack_seq = 0;
    }
    else if(flags == TH_ACK)
    {
        tcp_header->seq = htonl(*ack_num);
        *previous_seq += 1;
        tcp_header->ack_seq = htonl(*previous_seq);
    }
    else if(flags == TH_PUSH + TH_ACK)
    {
        tcp_header->seq = htonl(*ack_num);
        tcp_header->ack_seq = htonl(*previous_seq);
    }
    tcp_header->doff = 5;   // If no options, the size of tcp is 4 bytes * 5. That means tcp header is consist of 5 words.
    tcp_header->res1 = 0;   // This is reserved for later.

    // Flags
    // tcp_header->cwr = 0; // Congestion Window Reduced. This is related to ECN(Explicit Congestion Notification) in order to notice the status of congestion in the network.
    // tcp_header->ece = 0; // ECN-echo. This flag has specific relationship with syn.
    //                      // If syn = 1 and ece = 1, it means that sender is willing to use ECN in the connection.
    //                      // If syn = 0 and congestion occurs, ece is set to notice congestion to sender.
    // tcp_header->res2 = 0;    // I think this field has cwr and ece.
    // tcp_header->urg = 0; // Urgent Pointer. Setting this flag to 1 means receiver should process this packet with higher priority than others.
    // tcp_header->ack = 0; 
    // tcp_header->psh = 0; // Push Function. This field make the process of transmission as fast as possible.
    // tcp_header->rst = 0; // Reset. If this flag is 1, the connection is forcibly disconnected.
    // tcp_header->syn = 1; // This field is what I want now.
    // tcp_header->fin = 0; // Finish. This is used to finish the connection normally.
    tcp_header->th_flags = flags;

    tcp_header->window = htons(14480);  // Window size. The value is recommanded by ChatGPT.
    tcp_header->check = 0;
    tcp_header->urg_ptr = 0;            // This field has meaning only when urg is set to 1.
}

void create_pkt(char * buffer, int * pkt_len, struct sockaddr_in * saddr, struct sockaddr_in * daddr, struct iphdr * ip_header, struct tcphdr * tcp_header, uint16_t * packet_id, int flags, uint32_t * previous_seq, uint32_t * ack_num, char * payload, int payload_len)
{
    create_ip_header(ip_header, packet_id, saddr, daddr, payload_len);
    create_tcp_header(tcp_header, saddr, daddr, flags, previous_seq, ack_num);

    if(flags == TH_PUSH + TH_ACK)
        memcpy(buffer + sizeof(struct iphdr) + sizeof(struct tcphdr), payload, payload_len);

    struct pseudo_header ph;
    ph.src_addr_ip = saddr->sin_addr.s_addr;
    ph.dest_addr_ip = daddr->sin_addr.s_addr;
    ph.placeholder = 0;
    ph.protocol = IPPROTO_TCP;
    ph.tcp_seg_length = htons(sizeof(struct tcphdr) + payload_len);

    int total_tcp_length = sizeof(struct tcphdr) + sizeof(struct pseudo_header) + payload_len;
    char * tcp_checksum_datagram = malloc(total_tcp_length);

	// To assign the pseudo_header's address to tcp_checksum_datagram
    memcpy(tcp_checksum_datagram, &ph, sizeof(struct pseudo_header));
	// To assign the tcp_header's address to tcp_checksum_datagram behind the pseudo_header
    memcpy(tcp_checksum_datagram + sizeof(struct pseudo_header), tcp_header, sizeof(struct tcphdr));
    if(payload_len > 0)
        memcpy(tcp_checksum_datagram + sizeof(struct pseudo_header) + sizeof(struct tcphdr), payload, payload_len);

    tcp_header->check = checksum(tcp_checksum_datagram, total_tcp_length);
    ip_header->check = checksum((unsigned char *)buffer, ip_header->ihl * 4);

    *pkt_len = ip_header->tot_len;

    free(tcp_checksum_datagram);
}
