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

char *DNS(char url[888], char *ipstr)
{
    struct addrinfo hints;
    struct addrinfo *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    printf("url: %s\n", url);
    getaddrinfo(url, NULL, &hints, &servinfo);

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)servinfo->ai_addr;
    void *addr = &(ipv4->sin_addr);
    inet_ntop(hints.ai_family, addr, ipstr, INET6_ADDRSTRLEN);
    printf("IP(func): %s\n", ipstr);
    return ipstr;
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
        printf("Server getaddrinfo error\n");
        exit(1);
    }

    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        printf("Server socket error\n");
        exit(1);
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        close(sockfd);
        printf("Server bind error\n");
        exit(1);
    }
    freeaddrinfo(servinfo);

    printf("\033[33mServer is ready......\033[m\n\n");
    their_addr_len = sizeof their_addr;

    while (1)
    {
        Package package;
        // 3 way handshake (receive SYN)
        if ((numbytes = recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len)) == -1)
        {
            perror("Receive ERROR\n");
            continue;
        }

        // creat a port for client
        int tmp = atoi(SERVERPORT);
        sprintf(SERVERPORT, "%d", tmp + 1);

        // let child process handle client
        int pid = fork();

        if (!pid)
        {
            //printf("listener: got packet from %s\n", inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof s));
            printf("\tReceive a package (SYN)\n");

            int client_isn = package.seq_num;
            reset(&package);
            package.SYN = 1;
            package.seq_num = rand() % 10000 + 1;
            package.ack_num = client_isn + 1;
            strncpy(package.data, SERVERPORT, sizeof(package.data));
            sendto(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, their_addr_len);
            // 3 way handshake finish

            if ((rv = getaddrinfo(NULL, SERVERPORT, &hints, &servinfo)) != 0)
            {
                printf("Connet getaddrinfo error\n");
                exit(1);
            }

            if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
            {
                printf("Connet socket error\n");
                exit(1);
            }

            if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
            {
                close(sockfd);
                printf("Connet bind error\n");
                exit(1);
            }

            numbytes = recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
            printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", package.seq_num, package.ack_num);

            int num = 0;
            sscanf(package.data, "%d", &num); // receive request num

            /* test DNS */
            char *pch = strtok(package.data, " ");
            for (int i = 0; i < 2*num; i++)
            {
                pch = strtok(NULL, " ");
                printf("%s\n", pch);
            }

            char ipstr[INET6_ADDRSTRLEN];
            printf("IP: %s\n", DNS(pch, ipstr));
            /* test DNS */

            while (1)
            {
                numbytes = recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
                printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", package.seq_num, package.ack_num);
                printf("\tReceive : %s\n", package.data);
            }

            exit(0);
        }
    }

    return 0;
}