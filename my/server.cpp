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
/*
lsof -i :12000
kill -9 19422
*/
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

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(void)
{
    char SERVERPORT[5] = "4950"; // 使用者所要連線的 port
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t their_addr_len;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // 使用我的 IP

    if ((rv = getaddrinfo(NULL, SERVERPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // 用迴圈來找出全部的結果，並 bind 到首先找到能 bind 的
    for (p = servinfo; p != NULL; p = p->ai_next)
    {

        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);
    printf("listener: waiting to recvfrom...\n");
    their_addr_len = sizeof their_addr;

    while (1)
    {
        Package package;
        cout << "parent socket: " << sockfd << endl;
        if ((numbytes = recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len)) == -1)
        {
            perror("Receive ERROR\n");
            exit(1);
        }

        int tmp = atoi(SERVERPORT);
        sprintf(SERVERPORT, "%d", tmp + 1);

        int pid = fork();

        if (!pid)
        {
            printf("listener: got packet from %s\n", inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));
            printf("listener: packet is %d bytes long\n", numbytes);

            buf[numbytes] = '\0';
            if (package.SYN)
                printf("receive SYN\n");
            else
                printf("Receive : %s\n", package.data);

            int client_isn = package.seq_num;
            reset(&package);
            package.SYN = 1;
            package.seq_num = rand() % 10000 + 1;
            package.ack_num = client_isn + 1;
            strncpy(package.data, SERVERPORT, sizeof(package.data));
            sendto(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, their_addr_len);

            struct addrinfo hints, *servinfo, *p;
            int rv, numbytes;
            struct sockaddr_storage their_addr;
            socklen_t their_addr_len;
            char s[INET6_ADDRSTRLEN];

            memset(&hints, 0, sizeof hints);

            hints.ai_family = AF_INET; // IPv4
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_flags = AI_PASSIVE; // 使用我的 IP

            if ((rv = getaddrinfo(NULL, SERVERPORT, &hints, &servinfo)) != 0)
            {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                return 1;
            }

            // 用迴圈來找出全部的結果，並 bind 到首先找到能 bind 的
            p = servinfo;
            if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            {
                perror("listener: socket");
            }

            if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
            {
                close(sockfd);
                perror("listener: bind");
            }

            numbytes = recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
            printf("Receive: %s\n", package.data);

            while (1)
            {
                numbytes = recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
                printf("Receive: %s\n", package.data);
            }

            exit(0);
        }
    }

    return 0;
}