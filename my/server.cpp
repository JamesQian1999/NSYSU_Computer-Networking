#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <time.h>
#include <math.h>
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
#define LOSS 10e-6

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

int main(void)
{
    char SERVERPORT[5] = "4950"; // receiving port
    int sockfd, rv, numbytes;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t their_addr_len;
    char buf[MAXBUFLEN], s[INET6_ADDRSTRLEN];

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

    printf("\033[33mServer is ready......\033[m\n");
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
            printf("\tReceive a package (SYN)\n");
            srand(time(NULL));
            int SEQ = rand() % 10000 + 1, ACK;
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

            printf("request: %s\n", package.data);
            char *pch = strtok(package.data, " ");
            for (int i = 1; i <= num; i++)
            {
                Package sent_package, received_package;
                char *flag, *option;
                pch = strtok(NULL, " ");
                flag = pch;
                //cout << "fag = " << flag << endl;
                if (flag[1] == 'f') // e.g. -f 1.mp4
                {
                    int eof = 0, file_ptr = 1;
                    char tmp[100] = {0};
                    pch = strtok(NULL, " ");
                    sprintf(tmp, "%s", pch);
                    fstream file;
                    file.open(tmp, ios::in | ios::binary);

                    if (file.fail()) // File didn't exist.
                    {
                        printf("File %s didn't exist.\n", tmp);
                        strcpy(sent_package.data, "File didn't exist.");
                        sent_package.ack_num = ++ACK;
                        sent_package.seq_num = ++SEQ;
                        sent_package.FIN = 1;
                        sendto(sockfd, (char *)&sent_package, sizeof(sent_package), 0, (struct sockaddr *)&their_addr, their_addr_len);
                    }

                    char sent_byte;
                    int count = 1, tmp_count = 1;
                    while (file.get(sent_byte))
                    {
                        sent_package.data[count - 1] = sent_byte;

                        if (count == 1024)
                        {
                            sent_package.seq_num = ++SEQ;
                            sent_package.ack_num = ++ACK;
                            if (distribution(generator))
                                sent_package.check_sum = 1;
                        resent:
                            sendto(sockfd, (char *)&sent_package, sizeof(sent_package), 0, (struct sockaddr *)&their_addr, their_addr_len);
                            char s[INET6_ADDRSTRLEN];
                            inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
                            printf("Sent a package to %s ( seq_num = %u, ack_num = %u ): \n", s,sent_package.seq_num,sent_package.ack_num);

                            recvfrom(sockfd, (char *)&received_package, sizeof(received_package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
                            printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", received_package.seq_num, received_package.ack_num);
                            if (received_package.ack_num == SEQ)
                            {
                                sent_package.check_sum = 0;
                                goto resent;
                            }
                            count = 1;
                            reset(&sent_package);
                            continue;
                        }
                        count++;
                    }
                    sent_package.seq_num = ++SEQ;
                    sent_package.ack_num = ++ACK;
                    sent_package.data_size = count - 1;
                    sent_package.FIN = 1;
                    sendto(sockfd, (char *)&sent_package, sizeof(sent_package), 0, (struct sockaddr *)&their_addr, their_addr_len);
                    char s[INET6_ADDRSTRLEN];
                    inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
                    printf("Sent a package to %s : \n", s);

                    recvfrom(sockfd, (char *)&received_package, sizeof(received_package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
                    printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", received_package.seq_num, received_package.ack_num);
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

                    recvfrom(sockfd, (char *)&received_package, sizeof(received_package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
                    printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", received_package.seq_num, received_package.ack_num);
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
                        caculate(&sent_package, pch);
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

                    recvfrom(sockfd, (char *)&received_package, sizeof(received_package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
                    printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", received_package.seq_num, received_package.ack_num);
                }
            }
            printf("\033[32mClient %d transmit successful.\033[m\n", getpid());
            exit(0);
        }
    }

    return 0;
}