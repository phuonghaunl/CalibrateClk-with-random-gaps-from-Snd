#define _MULTI_THREADED
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/net_tstamp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include<pthread.h>
#include <signal.h>
#define BUFLEN 1542
#define PORT 9001
#define NUM_PKT 100000

void send_trains(char *serverIP, int number, int pktsize, double duration)
{
    struct sockaddr_in serv_addr;
    int sockfd, slen=sizeof(serv_addr);
    int i;
    char buf[BUFLEN];
    struct timeval t0, t1, t2, t3;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
        err("socket");
    int timestamp_flags=SOF_TIMESTAMPING_TX_SOFTWARE;
    if(setsockopt(sockfd, SOL_SOCKET,SO_TIMESTAMPING,&timestamp_flags,sizeof(timestamp_flags))<0) {
        err("timestamp error");
    }
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_aton(serverIP, &serv_addr.sin_addr)==0){
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
    long time[NUM_PKT+2];  // max 100000 packets
    if (number >= NUM_PKT+2){
        number = NUM_PKT+2;
        printf("increase time array size!\n");
    }
    gettimeofday(&t0, NULL);
    for( i=1;i<=number;i++)
    {
        gettimeofday(&t1,NULL);
        //sprintf(buf, "%d, %ld, %ld\n", -i, t1.tv_sec, t1.tv_usec);
        time[i]=(t1.tv_sec-t0.tv_sec)*1000000+(t1.tv_usec-t0.tv_usec);
	sprintf(buf, "%d, %ld\n", -i, time[i]);
	printf("snd time[%d]= %ld\n", i, time[i]);
        if (sendto(sockfd, buf, pktsize, 0,   (struct sockaddr*)&serv_addr, slen)==-1)
                    err("sendto()");
        gettimeofday(&t2, NULL);
        while((t2.tv_sec-t1.tv_sec)*1000000+(t2.tv_usec-t1.tv_usec)<duration-1)
            gettimeofday(&t2, NULL);

    }
    gettimeofday(&t3, NULL);
    long totduration = (t3.tv_sec-t0.tv_sec)*1000000+t3.tv_usec-t0.tv_usec;
    printf("takes time %ld us at rate %.2f Mbps\n", totduration, number*(pktsize+28)*8.0/totduration);
     sleep(1);
     //send a short packet to end the probing test
     if (sendto(sockfd, buf, 200, 0,   (struct sockaddr*)&serv_addr, slen)==-1)
                        err("sendto()");
   close(sockfd);
   long sndtimestamp[number]; sndtimestamp[1] = 0;
   /*for (i=2;i<=number;i++){
      // if ((time[i]-time[i-1])>2*duration)
      //      printf("sending interval: %ld us\n", time[i]-time[i-1]);
	sndtimestamp[i] = sndtimestamp[i-1] + time[i] - time[i-1];
        printf("sndtimestamp [%d] = %ld us\n", i, sndtimestamp[i] );
   }*/

}

int main(int argc, char* argv[])
{
    int num=500;//default 500 packets
    double rate=100; // default 100 Mbps
    double gap;
    int pktsize=300;//1472; //trying with pktsize=500, at first 1472
    char *serverIP=argv[1];
    if(argc>1) num=atoi(argv[2]);
    if(argc>2) rate=atof(argv[3]);
   // if(argc>3) pktsize=atoi(argv[4]);
    gap = (pktsize+28)*8/rate;
    printf("send %d pkts of %d to %s at rate %.4f Mbps (interval %.1f us) ", num, pktsize, serverIP, rate, gap);
    send_trains(serverIP,  num, pktsize, gap); //sending process
    return 0;
}

