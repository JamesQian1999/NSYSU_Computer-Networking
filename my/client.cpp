#include <unistd.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
using namespace std;

#define MAXBUFLEN 100

class Package
{
public:
    unsigned short destination_port = 0;
    unsigned short source_port = 0;
    unsigned int seq_num = 0;
    unsigned int ack_num = 0;
    unsigned char header_length = 0;
    bool ACK = 0;
    bool SYN = 0;
    bool FIN = 0;
    unsigned short window_size = 0;
    char data[1024];
};

void reset(Package *p)
{
    p->destination_port = 0;
    p->source_port = 0;
    p->seq_num = 0;
    p->ack_num = 0;
    p->header_length = 0;
    p->ACK = 0;
    p->SYN = 0;
    p->FIN = 0;
    p->window_size = 0;
    memset(&p->data, 0, sizeof p->data);
}

int main(int argc, char *argv[])
{
    char SERVERPORT[5] = "4950"; // 使用者所要連線的 port
    int sockfd;
    struct addrinfo hints, *servinfo;
    int rv;
    int numbytes;

    if (argc < 2 || argc % 2) //172.20.10.7 -f 1.mp4 -d www.google.com
    {
        printf("Invaild format.\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo)) != 0)
    {
        printf("getaddrinfo error\n");
        exit(1);
    }

    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        printf("Server socket error\n");
        exit(1);
    }
    freeaddrinfo(servinfo);

    //3 way handshake
    printf("\033[33m=====Start the three-way handshake======\033[m\n");
    printf("Sent package(SYN) to %s : %s\n", argv[1], SERVERPORT);
    Package handshake, package;
    handshake.SYN = 1;
    handshake.seq_num = rand() % 10000 + 1;
    if ((numbytes = sendto(sockfd, (char *)&handshake, sizeof(handshake), 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
    {
        printf("3-Way-Handshake ERROR\n");
        exit(1);
    }
    else
    {
        reset(&handshake);
        struct sockaddr_storage their_addr;
        socklen_t their_addr_len;
        numbytes = recvfrom(sockfd, (char *)&handshake, sizeof(handshake), 0, (struct sockaddr *)&their_addr, &their_addr_len);
        printf("Receive package(SYN/ACK) from %s : %s\n", argv[1], SERVERPORT);
        printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", handshake.seq_num, handshake.ack_num);
        printf("\033[33m====Complete the three-way handshake====\033[m\n");

        strncpy(SERVERPORT, handshake.data, sizeof(SERVERPORT) - 1);
        getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo);
        sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

        //sent request
        char num[50] = {0};
        sprintf(num, "%d", (argc - 2) / 2);     //caculate request num 
        strcat(package.data, num);              //request num
        for (int i = 2; i < argc; i++)
        {
            strcat(package.data, " ");
            strcat(package.data, argv[i]);
        }

        sendto(sockfd, (char *)&package, sizeof(package), 0, servinfo->ai_addr, servinfo->ai_addrlen);
    }

    while (1)
    {
        char msg[100];
        cout << "Enter the message: ";
        cin >> msg;
        Package package;
        strncpy(package.data, msg, sizeof(package.data) - 1);
        if ((numbytes = sendto(sockfd, (char *)&package, sizeof(package), 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
        {

            perror("talker: sendto");
            exit(1);
        }
        printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
        char buf[MAXBUFLEN];
        struct sockaddr_storage their_addr;
        socklen_t their_addr_len = sizeof their_addr;
        //numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr *)&their_addr, &their_addr_len);
    }

    close(sockfd);
    return 0;
}