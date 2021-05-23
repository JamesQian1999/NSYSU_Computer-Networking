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
    unsigned short int destination_port = 0;
    unsigned short int source_port = 0;
    unsigned int seq_num = 0;
    unsigned int ack_num = 0;
    unsigned char header_length = 0;
    unsigned char reserved1 = 0;
    bool reserved2 = 0;
    bool reserved3 = 0;
    bool URG = 0;
    bool ACK = 0;
    bool PSH = 0;
    bool PST = 0;
    bool SYN = 0;
    bool FIN = 0;
    unsigned short window_size = 0;
    unsigned short check_sum = 0;
    unsigned short urgent_pointer = 0;
    char data[1024];
};

void reset(Package *p)
{
    p->destination_port = 0;
    p->source_port = 0;
    p->seq_num = 0;
    p->ack_num = 0;
    p->header_length = 0;
    p->reserved1 = 0;
    p->reserved2 = 0;
    p->reserved3 = 0;
    p->URG = 0;
    p->ACK = 0;
    p->PSH = 0;
    p->PST = 0;
    p->SYN = 0;
    p->FIN = 0;
    p->window_size = 0;
    p->check_sum = 0;
    p->urgent_pointer = 0;
    memset(&p->data, 0, sizeof p->data);
}

int main(int argc, char *argv[])
{
    char SERVERPORT[5] = "4950"; // 使用者所要連線的 port
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    if (argc < 2 || argc % 2) //172.20.10.7 -f 1.mp4 -d www.google.com
    {
        fprintf(stderr, "Invaild format.\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    p = servinfo;
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
        perror("talker: socket");
    }
    freeaddrinfo(servinfo);

    if (p == NULL)
    {
        fprintf(stderr, "talker: failed to bind socket\n");
        return 2;
    }

    //3 way handshake
    Package handshake;
    handshake.SYN = 1;
    handshake.seq_num = rand() % 10000 + 1;
    cout << "before sockfd = " << sockfd << endl;
    printf("sent ==> seq num: %d\n", handshake.seq_num);
    if ((numbytes = sendto(sockfd, (char *)&handshake, sizeof(handshake), 0, p->ai_addr, p->ai_addrlen)) == -1)
    {
        perror("3-Way-Handshake ERROR\n");
        exit(1);
    }
    else
    {
        reset(&handshake);
        struct sockaddr_storage their_addr;
        socklen_t their_addr_len;
        numbytes = recvfrom(sockfd, (char *)&handshake, sizeof(handshake), 0, (struct sockaddr *)&their_addr, &their_addr_len);
        printf("receive ==> ack num:%d, seq num: %d, new port: %s\n", handshake.ack_num, handshake.seq_num, handshake.data);

        strncpy(SERVERPORT, handshake.data, sizeof(SERVERPORT) - 1);
        cout << "port: " << SERVERPORT << endl;
        cout << getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo) << endl;
        sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
        cout << "after sockfd = " << sockfd << endl;

        strncpy(handshake.data, "hello world\n", sizeof(handshake.data) - 1);
        sendto(sockfd, (char *)&handshake, sizeof(handshake), 0, servinfo->ai_addr, servinfo->ai_addrlen);
        cout << "here" << endl;
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
        buf[numbytes] = '\0';
        //printf("Receive : %s\n", buf);
    }

    close(sockfd);
    return 0;
}