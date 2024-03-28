#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <arpa/inet.h> 

#define BUFFER_SIZE 65536

void print_ethernet_header(char* buffer);
void print_ip_header(char* buffer);
void print_tcp_packet(char* buffer);

int main(int argc, char *argv[]) {
    int raw_socket;
    int num_packets;
    char buffer[BUFFER_SIZE];
    char *interface_name = NULL; 
    struct ifreq ifr;

    if (argc < 2) {
        printf("Usage: %s <Network Interface> \n", argv[0]);
        return -1;
    }

    interface_name = argv[1]; 

    raw_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (raw_socket < 0) {
        perror("Failed to create raw socket");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, sizeof(ifr.ifr_name) - 1);
    if (setsockopt(raw_socket, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0) {
        perror("Failed to bind raw socket to interface");
        close(raw_socket);
        return -1;
    }

    num_packets = 0;
    while (1) {
        ssize_t length = recv(raw_socket, buffer, BUFFER_SIZE, 0);
        if (length < 0) {
            perror("Failed to receive frame");
            break;
        }

        printf("============================================ \n"); 
        printf("Packet No: %d \n", num_packets);  
        print_ethernet_header(buffer);
        printf("============================================ \n\n"); 

        num_packets++;        
    }

    close(raw_socket);

    return 0;
}


void print_ethernet_header(char* buffer) {
    struct ethhdr *eth = (struct ethhdr*)buffer;

    printf("-------------------------------------------- \n");
    printf("Ethernet Header\n");
    printf("   |-Source MAC Address      : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X \n", 
            eth->h_source[0], eth->h_source[1], eth->h_source[2], 
            eth->h_source[3], eth->h_source[4], eth->h_source[5]);
    printf("   |-Destination MAC Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X \n", 
            eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], 
            eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
    printf("   |-Protocol                : %u \n", (unsigned short)eth->h_proto);

    // Check if the next layer is an IP packet based on the EtherType
    // TODO
    if(ntohs(eth->h_proto) == 0x0800) {
        print_ip_header(buffer + sizeof(struct ethhdr));
    }
}

void print_ip_header(char* buffer) {
    // TODO 
    // Hint: struct iphdr
    struct iphdr * ip = (struct iphdr*)buffer;

    // // To change IP address to human-readable format
    struct in_addr src_ip_adr, dest_ip_adr;

    src_ip_adr.s_addr = ip->saddr; 
    dest_ip_adr.s_addr = ip->daddr;

    printf("-------------------------------------------- \n");
    printf("IP Header\n");
    printf("   |-Source IP Address      : %s \n", inet_ntoa(src_ip_adr));
    printf("   |-Destination IP Address : %s \n", inet_ntoa(dest_ip_adr));
    printf("   |-Protocol               : %u \n", (unsigned short)ip->protocol);

    if(ip->protocol == 6) {
        print_tcp_packet(buffer + sizeof(struct iphdr));
    }
}

void print_tcp_packet(char* buffer) {
    // TODO 
    // Hint: struct tcphdr 
    struct tcphdr * tcp = (struct tcphdr*)buffer;

    printf("-------------------------------------------- \n");
    printf("TCP Header\n");
    printf("   |-Source Port        : %u \n", tcp->source);
    printf("   |-Destination Port   : %u \n", tcp->dest);
    printf("   |-Sequence Number    : %u \n", tcp->seq);
    printf("   |-Acknowledge Number : %u \n", tcp->ack_seq);
    printf("   |-Flags : ");

    if (tcp->ack) {
        if (tcp->syn) {
            printf("ACK SYN \n");
        } else if (tcp->psh) {
            printf("ACK PSH \n");
        } else if (tcp->fin) {
            printf("ACK FIN \n");
        } else if (tcp->rst) {
            printf("ACK RST \n");
        } else {
            printf("ACK \n");
        }
    } else if (tcp->urg) {
        printf("URG \n");
    } else if (tcp->psh) {
        printf("PSH \n");
    } else if (tcp->rst) {
        printf("RST \n");
    } else if (tcp->syn) {
        printf("SYN \n");
    } else if (tcp->fin) {
        printf("FIN \n");
    }
}
