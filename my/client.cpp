#include <unistd.h>
#include <iostream>
#include <fstream>
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
    unsigned int check_sum = 0;
    unsigned short data_size = 1024;
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
    p->check_sum = 0;
    p->data_size = 1024;
    p->ACK = 0;
    p->SYN = 0;
    p->FIN = 0;
    p->window_size = 0;
    memset(&p->data, 0, sizeof(p->data));
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    char SERVERPORT[5] = "4950", SERVERPORT_[5] = "4950"; // server port
    int sockfd, rv, numbytes, SEQ = rand() % 10000 + 1, ACK;
    struct addrinfo hints, *servinfo;
    struct sockaddr_storage their_addr;
    socklen_t their_addr_len;

    if (argc < 2 || argc % 2) //172.20.10.7 -f 1.mp4 -DNS www.google.com
    {
        printf("Invaild format.\n");
        exit(1);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo)) != 0)
    {
        printf("getaddrinfo error.\n");
        exit(1);
    }

    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        printf("Server socket error.\n");
        exit(1);
    }
    freeaddrinfo(servinfo);

    //3-way handshake START
    printf("\033[33m=====Start the three-way handshake======\033[m\n");
    printf("Sent package(SYN) to %s : %s\n", argv[1], SERVERPORT);
    Package handshake, package;
    handshake.SYN = 1;
    handshake.seq_num = SEQ;
    if ((numbytes = sendto(sockfd, (char *)&handshake, sizeof(handshake), 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
    {
        printf("3-Way-Handshake ERROR\n");
        exit(1);
    }
    else
    {
        reset(&handshake);
        numbytes = recvfrom(sockfd, (char *)&handshake, sizeof(handshake), 0, (struct sockaddr *)&their_addr, &their_addr_len);
        printf("Receive package(SYN/ACK) from %s : %s\n", argv[1], SERVERPORT);
        printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", handshake.seq_num, handshake.ack_num);
        ACK = ++handshake.seq_num;

        strncpy(SERVERPORT, handshake.data, sizeof(SERVERPORT) - 1);
        getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo);
        sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

        //sent request
        reset(&package);
        char num[50] = {0};
        sprintf(num, "%d", (argc - 2) / 2); //caculate request num
        strcat(package.data, num);          //request num
        for (int i = 2; i < argc; i++)
        {
            strcat(package.data, " ");
            strcat(package.data, argv[i]);
        }

        package.seq_num = ++SEQ;
        package.ack_num = ACK;
        sendto(sockfd, (char *)&package, sizeof(package), 0, servinfo->ai_addr, servinfo->ai_addrlen);
        reset(&package);

        char s[INET6_ADDRSTRLEN];
        inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
        printf("Sent package(SYN) to %s : %s\n", s, SERVERPORT_);
        printf("\033[33m====Complete the three-way handshake====\033[m\n");
    }
    //3-way handshake END

    for (int i = 1; i <= (argc - 2) / 2; i++)
    {
        char flag[5] = {0}, option[500] = {0}, s[INET6_ADDRSTRLEN];
        strcat(flag, argv[i * 2]);
        strcat(option, argv[i * 2 + 1]);
        //cout << "flag = " << flag << ",  option = " << option << endl;
        if (flag[1] == 'f') // e.g. -f 1.mp4
        {
            printf("\033[32mReceiving %s form %s : %s\033[m\n", option, argv[1], SERVERPORT_);
            char file_name[20] = {0};
            strcpy(file_name, "received_");
            strcat(file_name, option);

            fstream file;
            file.open(file_name, ios::out | ios::binary);
            int FIN = 0;

            while (!FIN)
            {
                recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
                ACK = package.seq_num;
                if (package.check_sum)
                {
                    reset(&package);
                    package.seq_num = ++SEQ;
                    package.ack_num = ACK;
                    sendto(sockfd, (char *)&package, sizeof(package), 0, servinfo->ai_addr, servinfo->ai_addrlen);
                    continue;
                }
                FIN = package.FIN;
                printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", package.seq_num, package.ack_num);
                for (int i = 0; i < package.data_size; i++)
                    file << package.data[i];

                reset(&package);
                package.seq_num = ++SEQ;
                package.ack_num = ++ACK;
                sendto(sockfd, (char *)&package, sizeof(package), 0, servinfo->ai_addr, servinfo->ai_addrlen);
            }
            printf("\tFinish receiving.\n");
            file.close();
        }
        else if (flag[1] == 'D' && flag[2] == 'N' && flag[3] == 'S') // e.g. -DNS google.com
        {
            recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
            inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
            printf("\033[32mReceive a DNS result from %s : %s\033[m\n", s, SERVERPORT_);
            printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", package.seq_num, package.ack_num);
            printf("\treceived: %s\n", package.data);
            ACK = package.seq_num;
            reset(&package);
            package.seq_num = ++SEQ;
            package.ack_num = ++ACK;
            sendto(sockfd, (char *)&package, sizeof(package), 0, servinfo->ai_addr, servinfo->ai_addrlen);
        }
        else if (
            (flag[1] == 'a' && flag[2] == 'd' && flag[3] == 'd')    // e.g. -add 12+23
            || (flag[1] == 's' && flag[2] == 'u' && flag[3] == 'b') // e.g. -sub 12-23
            || (flag[1] == 'm' && flag[2] == 'u' && flag[3] == 'l') // e.g. -mul -2*2
            || (flag[1] == 'd' && flag[2] == 'i' && flag[3] == 'v') // e.g. -div 9/2
            || (flag[1] == 'p' && flag[2] == 'o' && flag[3] == 'w') // e.g. -pow 5^2
            || (flag[1] == 's' && flag[2] == 'q' && flag[3] == 'r') // e.g. -sqrt 2
        )
        {
            recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
            inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
            printf("\033[32mReceive a calculation result from %s : %s\033[m\n", s, SERVERPORT_);
            printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", package.seq_num, package.ack_num);
            printf("\treceived: %s\n", package.data);
            ACK = package.seq_num;
            reset(&package);
            package.seq_num = ++SEQ;
            package.ack_num = ++ACK;
            sendto(sockfd, (char *)&package, sizeof(package), 0, servinfo->ai_addr, servinfo->ai_addrlen);
        }
        else // error
        {
            printf("Invaild flag.\n");
            continue;
        }
    }

    close(sockfd);
    return 0;
}