#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <thread>
#include <mutex>
#include <queue>
#include <unistd.h>
#include <time.h>
#include <math.h>
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
/*
lsof -i :12000
kill -9 19422
*/

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

#define MAXBUFLEN 512
#define LOSS 10e-6
#define EMPTY 0
#define RCV 1
#define SENT 2
#define ACKed 3
#define slow_start 1
#define congestion_avoidance 2
#define fast_recovery 3

void caculate(Package *sent_package, const char *pch, char op);
char *DNS(const char *url, char *ipstr);
void reset(Package *p);
void reset_buff(int num);
void receiving_pkg();
mutex myMutex;

//received buffer
int rcv_front = 0, rcv_tail = 0;
int rcv_buff_check[MAXBUFLEN] = {0};
Package rcv_buff[MAXBUFLEN];

//sent buffer
int sent_front[2] = {0, -1}, sent_tail = 0;
int sent_buff_check[MAXBUFLEN] = {0};
Package sent_buff[MAXBUFLEN];

int sockfd;
struct sockaddr_storage their_addr;
socklen_t their_addr_len;

int main(void)
{
    char SERVERPORT[5] = "4950"; // receiving port
    int rv, numbytes;
    struct addrinfo hints, *servinfo, *p;
    // struct sockaddr_storage their_addr;
    // socklen_t their_addr_len;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM; // UDP
    hints.ai_flags = AI_PASSIVE;    // my address

    if ((rv = getaddrinfo(NULL, SERVERPORT, &hints, &servinfo)) != 0)
    {
        printf("Server getaddrinfo error.\n");
        exit(1);
    }

    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        printf("Server socket error.\n");
        exit(1);
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        close(sockfd);
        printf("Server bind error.\n");
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
            perror("Receive error.\n");
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
            default_random_engine generator;
            bernoulli_distribution distribution(LOSS);
            printf("\033[33m=====Three-way handshake(Client %d)======\033[m\n", getpid());
            printf("\tReceive a package (SYN)\n");
            srand(time(NULL));
            int SEQ = rand() % 10000 + 1, ACK, ssthresh = 8, MSS = 1, wnd = 1, state = slow_start; //rand() % 10000 + 1
            ACK = ++package.seq_num;
            reset(&package);
            package.SYN = 1;
            package.seq_num = SEQ;
            package.ack_num = ACK;
            strncpy(package.data, SERVERPORT, sizeof(package.data));
            sendto(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, their_addr_len);
            // 3 way handshake finish

            if ((rv = getaddrinfo(NULL, SERVERPORT, &hints, &servinfo)) != 0)
            {
                printf("Connet getaddrinfo error.\n");
                exit(1);
            }

            if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
            {
                printf("Connet socket error.\n");
                exit(1);
            }

            if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
            {
                close(sockfd);
                printf("Connet bind error.\n");
                exit(1);
            }

            numbytes = recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
            printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", package.seq_num, package.ack_num);

            int num = 0;
            sscanf(package.data, "%d", &num); // receive request num

            thread receiving(receiving_pkg);
            printf("request: %s\n", package.data);
            char *pch = strtok(package.data, " ");
            for (int i = 1; i <= num; i++)
            {
                Package sent_package;
                char *flag, *option;
                pch = strtok(NULL, " ");
                flag = pch;
                //cout << "fag = " << flag << endl;
                if (flag[1] == 'f') // e.g. -f 1.mp4
                {

                    char file_name[100] = {0};
                    pch = strtok(NULL, " ");
                    sprintf(file_name, "%s", pch);
                    int fd = open(file_name, O_RDONLY);
                    if (fd == -1)
                    {
                        printf("File %s didn't exist.\n", file_name);
                        strcpy(sent_package.data, "File didn't exist.");
                        sent_package.window_size = 1;
                        sent_package.ack_num = ++ACK;
                        sent_package.seq_num = ++SEQ;
                        sent_package.FIN = 1;
                        sendto(sockfd, (char *)&sent_package, sizeof(sent_package), 0, (struct sockaddr *)&their_addr, their_addr_len);
                        goto done;
                    }
                    //wnd = 1;
                    printf("\033[32mSending %s\n\033[m", file_name);
                    while (1)
                    {

                        char tmp[1024];
                        for (int i = 0; i < wnd; i++)
                        {

                            long int rv = read(fd, sent_buff[sent_tail].data, 1024);
                            //printf("No. %d, rv = %ld\n", sent_tail, rv);
                            sent_buff[sent_tail].data_size = rv;

                            sent_buff[sent_tail].destination_port = 0;
                            sent_buff[sent_tail].source_port = 0;
                            sent_buff[sent_tail].seq_num = ++SEQ;
                            //printf("\033[33msent_buff[%d].seq_num = %d\n\033[m", sent_tail, sent_buff[sent_tail].seq_num);
                            sent_buff[sent_tail].ack_num = 0;
                            sent_buff[sent_tail].check_sum = 0;
                            sent_buff[sent_tail].END = 0;
                            sent_buff[sent_tail].ACK = 0;
                            sent_buff[sent_tail].SYN = 0;
                            sent_buff[sent_tail].FIN = 0;
                            sent_buff[sent_tail].window_size = wnd;

                            sent_buff_check[sent_tail] = RCV;
                            if (!i)
                                sent_front[1] = SEQ;

                            //printf("sent_front = %d, sent_tail = %d, rv = %ld\n", sent_front[0], sent_tail, rv);

                            if (rv < 1024)
                            {
                                //printf("last size = %d\n",sent_buff[sent_tail].data_size);
                                sent_buff[sent_tail].FIN = 1;
                                sent_tail = (sent_tail + 1) % MAXBUFLEN;
                                break;
                            }
                            sent_tail = (sent_tail + 1) % MAXBUFLEN;
                        }

                        bool finish = 0;
                        int ptr = sent_front[0], finish_detected;
                        char s[INET6_ADDRSTRLEN];
                        inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
                        printf("cwnd = %d, ssthresh = %d\n", wnd, ssthresh);
                        for (int i = 0; i < wnd; i++)
                        {
                            printf("Sent a package at 1024 byte( seq_num = %u, ack_num = %u )\n", sent_buff[ptr].seq_num, sent_buff[ptr].ack_num);
                            sent_buff[ptr].ack_num = ++ACK;
                            //printf("\033[31msent_buff[%d].seq_num = %d\n\033[m", ptr, sent_buff[ptr].ack_num);
                            finish = sent_buff[ptr].FIN;
                            finish_detected = ptr;
                            if ((wnd == 4 && i == wnd - 1) || (wnd == 8 && i == wnd - 1))
                                sent_buff[ptr].ACK = 1;
                            sendto(sockfd, (char *)&sent_buff[ptr], sizeof(sent_buff[ptr]), 0, (struct sockaddr *)&their_addr, their_addr_len);
                            //printf("No. %d, FIN = %d\n", ptr, sent_buff[ptr].FIN);
                            //char s[INET6_ADDRSTRLEN];
                            //inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
                            if (finish)
                                break;
                            ptr = (ptr + 1) % MAXBUFLEN;
                            usleep(3);
                        }

                        for (int i = 0; i < wnd; i++)
                        {
                            //printf("sent_front[1] = %d\n", sent_front[1]);
                            while (rcv_buff_check[rcv_front] == EMPTY)
                                ;
                            if (rcv_buff[rcv_front].ACK)
                            {
                                printf("Receive three duplicate ACKs\n");
                                //show state
                                switch (state)
                                {
                                case slow_start:
                                    printf("\033[34m*****Fast recovery*****\n\033[m");
                                    state = fast_recovery;
                                    break;
                                case congestion_avoidance:
                                    printf("\033[34m*****Fast recovery*****\n\033[m");
                                    state = fast_recovery;
                                    break;
                                case fast_recovery:
                                    printf("\033[34m*****Congestion_avoidance*****\n\033[m");
                                    state = congestion_avoidance;
                                    break;
                                }
                            }
                            else
                            {
                                printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", rcv_buff[rcv_front].seq_num, rcv_buff[rcv_front].ack_num);
                                if (state == fast_recovery)
                                {
                                    printf("\033[34m*****Slow start*****\n\033[m");
                                    state = slow_start;
                                }
                            }
                            rcv_buff_check[rcv_front] = EMPTY;
                            //printf("rcv_buff[%d].ack_num = %d\n", rcv_front, rcv_buff[rcv_front].ack_num);
                            //printf("here1\n");
                            sent_buff_check[((rcv_buff[rcv_front].ack_num - 1) - sent_front[1]) % MAXBUFLEN] = ACKed;
                            //printf("here2\n");
                            rcv_front = (rcv_front + 1) % MAXBUFLEN;
                            //printf("Before No. %d, FIN = %d\n", sent_front[0], sent_buff[sent_front[0]].FIN);
                            int tmp = sent_front[0];
                            //printf("A No. %d, FIN = %d\n", sent_front[0], sent_buff[sent_front[0]].FIN);
                            sent_front[0] = (sent_front[0] + 1) % MAXBUFLEN;
                            myMutex.unlock();
                            if (wnd >= ssthresh)
                            {
                                state = congestion_avoidance;
                            }

                            if (finish_detected == tmp && sent_buff[finish_detected].FIN)
                            {
                                reset_buff(finish_detected);
                                goto done;
                            }
                        }
                        if (state == congestion_avoidance)
                        {
                            wnd = wnd + 1;
                        }
                        else if (state == slow_start)
                            wnd = 2 * wnd;
                    }
                done:;
                    close(fd);
                    //printf("done====================================================\n");
                }
                else if (flag[1] == 'D' && flag[2] == 'N' && flag[3] == 'S') // e.g. -DNS google.com
                {
                    pch = strtok(NULL, " ");
                    char ipstr[INET6_ADDRSTRLEN];
                    strcpy(sent_package.data, DNS(pch, ipstr));
                    //cout << "sent: " << sent_package.data << endl;
                    sent_package.seq_num = ++SEQ;
                    sent_package.ack_num = ++ACK;
                    sendto(sockfd, (char *)&sent_package, sizeof(sent_package), 0, (struct sockaddr *)&their_addr, their_addr_len);
                    char s[INET6_ADDRSTRLEN];
                    inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
                    printf("Sent a package to %s : \n", s);

                    //recvfrom(sockfd, (char *)&received_package, sizeof(received_package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
                    while (rcv_buff_check[rcv_front] == EMPTY)
                        ;
                    myMutex.lock();
                    printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", rcv_buff[rcv_front].seq_num, rcv_buff[rcv_front].ack_num);
                    rcv_buff_check[rcv_front] = EMPTY;
                    rcv_front = (rcv_front + 1) % MAXBUFLEN;
                    myMutex.unlock();
                }
                else
                {
                    pch = strtok(NULL, " ");

                    if (flag[1] == 'a' && flag[2] == 'd' && flag[3] == 'd') // e.g. -add 12+23
                        caculate(&sent_package, pch, '+');

                    else if (flag[1] == 's' && flag[2] == 'u' && flag[3] == 'b') // e.g. -sub 12-23
                        caculate(&sent_package, pch, '-');

                    else if (flag[1] == 'm' && flag[2] == 'u' && flag[3] == 'l') // e.g. -mul -2*2
                        caculate(&sent_package, pch, '*');

                    else if (flag[1] == 'd' && flag[2] == 'i' && flag[3] == 'v') // e.g. -div 9/2
                        caculate(&sent_package, pch, '/');

                    else if (flag[1] == 'p' && flag[2] == 'o' && flag[3] == 'w') // e.g. -pow
                        caculate(&sent_package, pch, '^');

                    else if (flag[1] == 's' && flag[2] == 'q' && flag[3] == 'r') // e.g. -sqr 2
                        caculate(&sent_package, pch, 0);
                    else // error
                    {
                        printf("Invaild flag.\n");
                        continue;
                    }
                    sent_package.seq_num = ++SEQ;
                    sent_package.ack_num = ++ACK;
                    sendto(sockfd, (char *)&sent_package, sizeof(sent_package), 0, (struct sockaddr *)&their_addr, their_addr_len);
                    char s[INET6_ADDRSTRLEN];
                    inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
                    printf("Sent a package to %s : \n", s);

                    while (rcv_buff_check[rcv_front] == EMPTY)
                        ;
                    myMutex.lock();
                    printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", rcv_buff[rcv_front].seq_num, rcv_buff[rcv_front].ack_num);
                    rcv_buff_check[rcv_front] = EMPTY;
                    rcv_front = (rcv_front + 1) % MAXBUFLEN;
                    myMutex.unlock();
                }
            }
            //printf("rcv_front = %d,  rcv_tail = %d\n", rcv_front, rcv_tail);
            receiving.join();
            printf("\033[32mClient %d transmit successful.\033[m\n\n", getpid());
            close(sockfd);
            exit(0);
        }
    }

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
            Package end;
            end.END = 1;
            sendto(sockfd, (char *)&end, sizeof(end), 0, (struct sockaddr *)&their_addr, their_addr_len);
            //printf("end!\n");
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

        rcv_tail = (rcv_tail + 1) % MAXBUFLEN;
        myMutex.unlock();
    }
}

void reset_buff(int num)
{
    sent_buff[num].destination_port = 0;
    sent_buff[num].source_port = 0;
    sent_buff[num].seq_num = 0;
    sent_buff[num].ack_num = 0;
    sent_buff[num].check_sum = 0;
    sent_buff[num].data_size = 0;
    sent_buff[num].END = 0;
    sent_buff[num].ACK = 0;
    sent_buff[num].SYN = 0;
    sent_buff[num].FIN = 0;
    sent_buff[num].window_size = 0;
    for (int i = 0; i < 1024; i++)
        sent_buff[num].data[i] = 0;
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

char *DNS(const char *url, char *ipstr)
{
    struct addrinfo hints;
    struct addrinfo *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(url, NULL, &hints, &servinfo) != 0)
    {
        strcpy(ipstr, "Invaild format or Invaild domain name.");
        return ipstr;
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)servinfo->ai_addr;
    void *addr = &(ipv4->sin_addr);
    inet_ntop(hints.ai_family, addr, ipstr, INET6_ADDRSTRLEN);
    return ipstr;
}

void caculate(Package *sent_package, const char *pch, char op = 0)
{
    char a[50] = {0}, b[50] = {0}, tmp[100], bit;
    int count = 0, operend = 0, after_op = 0, flag = 1;
    float a_f, b_f, ans;
    memset(tmp, '\0', 100);
    sprintf(tmp, "%s", pch);
    if (!op) // sqrt
    {
        sscanf(tmp, "%f", &a_f);
        sprintf(sent_package->data, "%.5f", sqrt(a_f));
        return;
    }
    while (tmp[count] != '\0')
    {
        if (tmp[count] == op && count != 0 && count != after_op)
        {
            after_op = ++count;
            flag = 0;
            operend = 0;
            continue;
        }
        if (flag)
            a[operend++] = tmp[count++];
        else
            b[operend++] = tmp[count++];
    }
    sscanf(a, "%f", &a_f);
    sscanf(b, "%f", &b_f);
    switch (op)
    {
    case '+':
        ans = a_f + b_f;
        break;
    case '-':
        ans = a_f - b_f;
        break;
    case '*':
        ans = a_f * b_f;
        break;
    case '/':
        ans = a_f / b_f;
        break;
    case '^':
        ans = pow(a_f, b_f);
        break;
    }
    if (flag)
        sprintf(sent_package->data, "%s", "error.");
    else
        sprintf(sent_package->data, "%.5f", ans);
}
