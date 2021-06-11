#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <unistd.h>
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

class Package
{
public:
    unsigned short destination_port = 0;
    unsigned short source_port = 0;
    unsigned int seq_num = 0;
    unsigned int ack_num = 0;
    unsigned int check_sum = 0;
    unsigned short data_size = 1024;
    bool END = 0;
    bool ACK = 0;
    bool SYN = 0;
    bool FIN = 0;
    unsigned short window_size = 0;
    char data[1024];
};

#define LOSS 10e-6
struct addrinfo hints, *servinfo;
struct sockaddr_storage their_addr;
socklen_t their_addr_len;
char SERVERPORT[5] = "4950", SERVERPORT_[5] = "4950"; // server port
int sockfd, rv, numbytes, SEQ = rand() % 10000 + 1, ACK;
mutex myMutex;

//received buffer
int rcv_front = 0, rcv_tail = -1;
bool rcv_buff_check[512] = {0};
Package rcv_buff[512];

void reset(Package *p);
void three_way_handshake(int argc, char *argv[]);
void TCP_receiving(int argc, char *argv[]);
void receiving_pkg();

int main(int argc, char *argv[])
{
    srand(time(NULL));
    three_way_handshake(argc, argv);
    TCP_receiving(argc, argv);
    printf("\033[33mClient request successful.\n\033[m");
}

void TCP_receiving(int argc, char *argv[])
{
    thread receiving(receiving_pkg);
    for (int i = 1; i <= (argc - 2) / 2; i++)
    {
        Package sent_pkg;
        char flag[5] = {0}, option[500] = {0}, s[INET6_ADDRSTRLEN];
        strcat(flag, argv[i * 2]);
        strcat(option, argv[i * 2 + 1]);
        //cout << "flag = " << flag << ",  option = " << option << endl;
        if (flag[1] == 'f') // e.g. -f 1.mp4
        {
        }
        else if (flag[1] == 'D' && flag[2] == 'N' && flag[3] == 'S') // e.g. -DNS google.com
        {
            printf("\033[32mReceive a DNS result from %s : %s\033[m\n", argv[1], SERVERPORT_);
            myMutex.lock();
            printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", rcv_buff[rcv_front].seq_num, rcv_buff[rcv_front].ack_num);
            printf("\tThe DNS result of \"%s\": %s\n", option, rcv_buff[rcv_front].data);
            sent_pkg.seq_num = ++SEQ;
            sent_pkg.ack_num = ++rcv_buff[rcv_front].seq_num;
            rcv_front = (rcv_front + 1) % 512;
            myMutex.unlock();
            sendto(sockfd, (char *)&sent_pkg, sizeof(sent_pkg), 0, servinfo->ai_addr, servinfo->ai_addrlen);
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
        }
        else // error
        {
            printf("Invaild flag.\n");
            continue;
        }
    }
    receiving.join();
}

void receiving_pkg()
{
    while (1)
    {
        Package received_package;
        recvfrom(sockfd, (char *)&received_package, sizeof(received_package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
        printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", received_package.seq_num, received_package.ack_num);
        myMutex.lock();
        rcv_tail = (rcv_tail + 1) % 512;
        rcv_buff[rcv_tail].destination_port = received_package.destination_port;
        rcv_buff[rcv_tail].source_port = received_package.source_port;
        rcv_buff[rcv_tail].seq_num = received_package.seq_num;
        rcv_buff[rcv_tail].ack_num = received_package.ack_num;
        rcv_buff[rcv_tail].check_sum = received_package.check_sum;
        rcv_buff[rcv_tail].data_size = received_package.data_size;
        rcv_buff[rcv_tail].END = received_package.END;
        rcv_buff[rcv_tail].ACK = received_package.ACK;
        rcv_buff[rcv_tail].SYN = received_package.SYN;
        rcv_buff[rcv_tail].FIN = received_package.FIN;
        rcv_buff[rcv_tail].window_size = received_package.window_size;
        strcpy(rcv_buff[rcv_tail].data, received_package.data);
        myMutex.unlock();
    }
}

void reset(Package *p)
{
    p->destination_port = 0;
    p->source_port = 0;
    p->seq_num = 0;
    p->ack_num = 0;
    p->check_sum = 0;
    p->data_size = 1024;
    p->END = 0;
    p->ACK = 0;
    p->SYN = 0;
    p->FIN = 0;
    p->window_size = 0;
    memset(&p->data, 0, sizeof(p->data));
}

void three_way_handshake(int argc, char *argv[])
{
    if (argc < 3 || argc % 2) //172.20.10.7 -f 1.mp4 -DNS www.google.com
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
    printf("==>%d<==\n",handshake.seq_num);
    if ((numbytes = sendto(sockfd, (char *)&handshake, sizeof(handshake), 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
    {
        printf("3-Way-Handshake ERROR\n");
        exit(1);
    }
    else
    {
        printf("numbytes = %d\n", numbytes);
        reset(&handshake);
        numbytes = recvfrom(sockfd, (char *)&handshake, sizeof(handshake), 0, (struct sockaddr *)&their_addr, &their_addr_len);
        char s[INET6_ADDRSTRLEN];
        inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
        printf("numbytes = %d\n",numbytes);
        printf("r package(SYN) from %s=====\n", s );
        printf("Receive package(SYN/ACK) from %s : %s\n", argv[1], SERVERPORT);
        printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", handshake.seq_num, handshake.ack_num);
        ACK = ++handshake.seq_num;

        strncpy(SERVERPORT, handshake.data, sizeof(SERVERPORT) - 1);
        printf("here1 ==>%u<==\n",handshake.seq_num);
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

        //char s[INET6_ADDRSTRLEN];
        inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
        printf("Sent package(SYN) to %s : %s\n", s, SERVERPORT_);
        printf("\033[33m====Complete the three-way handshake====\033[m\n");
    }
    //3-way handshake END
}
