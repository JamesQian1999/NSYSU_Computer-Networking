#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

using namespace std;

class Package
{
public:
    unsigned short destination_port = 0;
    unsigned short source_port = 0;
    unsigned int seq_num = 0;
    unsigned int ack_num = 0;
    unsigned int check_sum = 0;
    int data_size = 1024;
    bool END = 0;
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
    p->END = 0;
    p->ACK = 0;
    p->SYN = 0;
    p->FIN = 0;
    p->window_size = 0;
    memset(&p->data, 0, sizeof(p->data));
}

#define MAXBUFLEN 512
#define LOSS 10e-6
#define EMPTY 0
#define RCV 1
#define SENT 2
#define ACKed 3

//handle packet
void receiving_pkg();

// mutex lock
mutex myMutex;

//received buffer
int rcv_front = 0, rcv_tail = 0;
int rcv_buff_check[MAXBUFLEN] = {0};
Package rcv_buff[MAXBUFLEN];

//addrss and socket
struct sockaddr_storage their_addr;
socklen_t their_addr_len;
int sockfd;

int main(int argc, char *argv[])
{
    srand(3 * time(NULL));
    char SERVERPORT[5] = "4950", SERVERPORT_[5] = "4950"; // server port
    int rv, numbytes, SEQ = 1, ACK;      //rand() % 10000 + 1
    struct addrinfo hints, *servinfo;

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
    Package handshake;
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
        reset(&handshake);
        char num[50] = {0};
        sprintf(num, "%d", (argc - 2) / 2); //caculate request num
        strcat(handshake.data, num);        //request num
        for (int i = 2; i < argc; i++)
        {
            strcat(handshake.data, " ");
            strcat(handshake.data, argv[i]);
        }

        handshake.seq_num = ++SEQ;
        handshake.ack_num = ACK;
        sendto(sockfd, (char *)&handshake, sizeof(handshake), 0, servinfo->ai_addr, servinfo->ai_addrlen);
        reset(&handshake);

        char s[INET6_ADDRSTRLEN];
        inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
        printf("Sent package(SYN) to %s : %s\n", s, SERVERPORT_);
        printf("\033[33m====Complete the three-way handshake====\033[m\n");
    }
    //3-way handshake END

    thread receiving(receiving_pkg); //handle receiving

    for (int i = 1; i <= (argc - 2) / 2; i++)
    {
        char flag[5] = {0}, option[500] = {0}, s[INET6_ADDRSTRLEN];
        strcat(flag, argv[i * 2]);
        strcat(option, argv[i * 2 + 1]);
        Package sent_package;
        //cout << "flag = " << flag << ",  option = " << option << endl;
        if (flag[1] == 'f') // e.g. -f 1.mp4
        {
            printf("\033[32mReceiving %s form %s : %s\033[m\n", option, argv[1], SERVERPORT_);
            char file_name[20] = {0};
            strcpy(file_name, "received_");
            strcat(file_name, option);
            int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
            while (1)
            {
                while (rcv_buff_check[rcv_front] == EMPTY)
                    ;
                myMutex.lock();
                printf("\033[32mReceive a package from %s : %s\033[m\n", argv[1], SERVERPORT_);
                printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", rcv_buff[rcv_front].seq_num, rcv_buff[rcv_front].ack_num);
                ACK = rcv_buff[rcv_front].seq_num;

                write(fd, rcv_buff[rcv_front].data, rcv_buff[rcv_front].data_size);

                int finish = rcv_buff[rcv_front].data_size;
                rcv_buff_check[rcv_front] = EMPTY;
                rcv_front = (rcv_front + 1) % MAXBUFLEN;
                myMutex.unlock();

                sent_package.seq_num = ++SEQ;
                sent_package.ack_num = ++ACK;
                sendto(sockfd, (char *)&sent_package, sizeof(sent_package), 0, servinfo->ai_addr, servinfo->ai_addrlen);
                if (finish < 1024)
                    break;
                printf("here\n");
            }
            printf("here out\n");
        }
        else if (flag[1] == 'D' && flag[2] == 'N' && flag[3] == 'S') // e.g. -DNS google.com
        {
            while (rcv_buff_check[rcv_front] == EMPTY)
                ;
            myMutex.lock();
            printf("\033[32mReceive a DNS result from %s : %s\033[m\n", argv[1], SERVERPORT_);
            printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", rcv_buff[rcv_front].seq_num, rcv_buff[rcv_front].ack_num);
            printf("\tThe DNS result of \"%s\": %s\n", option, rcv_buff[rcv_front].data);
            ACK = rcv_buff[rcv_front].seq_num;
            rcv_buff_check[rcv_front] = EMPTY;
            rcv_front = (rcv_front + 1) % MAXBUFLEN;
            myMutex.unlock();
            sent_package.seq_num = ++SEQ;
            sent_package.ack_num = ++ACK;
            sendto(sockfd, (char *)&sent_package, sizeof(sent_package), 0, servinfo->ai_addr, servinfo->ai_addrlen);
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
            while (rcv_buff_check[rcv_front] == EMPTY)
                ;
            myMutex.lock();
            printf("\033[32mReceive a calculation result from %s : %s\033[m\n", argv[1], SERVERPORT_);
            printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", rcv_buff[rcv_front].seq_num, rcv_buff[rcv_front].ack_num);
            printf("\tCalculation result: %s = %s\n", option, rcv_buff[rcv_front].data);
            ACK = rcv_buff[rcv_front].seq_num;
            rcv_buff_check[rcv_front] = EMPTY;
            rcv_front = (rcv_front + 1) % MAXBUFLEN;
            myMutex.unlock();
            sent_package.seq_num = ++SEQ;
            sent_package.ack_num = ++ACK;
            sendto(sockfd, (char *)&sent_package, sizeof(sent_package), 0, servinfo->ai_addr, servinfo->ai_addrlen);
        }
        else // error
        {
            printf("Invaild flag.\n");
            continue;
        }
    }

    //END connect
    Package end;
    end.END = 1;
    sendto(sockfd, (char *)&end, sizeof(end), 0, servinfo->ai_addr, servinfo->ai_addrlen);
    receiving.join();
    return 0;
}

void receiving_pkg()
{
    while (1)
    {
        Package received_package;
        recvfrom(sockfd, (char *)&received_package, sizeof(received_package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
        //printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", received_package.seq_num, received_package.ack_num);
        if (received_package.END)
        {
            //printf("end!\n");
            close(sockfd);
            break;
        }
        myMutex.lock();
        rcv_buff_check[rcv_tail] = RCV;
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
        for (int i = 0; i < 1024; i++)
            rcv_buff[rcv_tail].data[i] = received_package.data[i];
        //strcpy(rcv_buff[rcv_tail].data, received_package.data);
        rcv_tail = (rcv_tail + 1) % MAXBUFLEN;
        myMutex.unlock();
    }
}