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
#include <time.h>
#include <math.h>
#define BUFLEN 1542
#define PORT 9001
#define PORT_ 9945

//generate random normal sending gaps
    double rands[300];

/*    for (i=1; i<=number; i++){
      rands[i] = (duration + 200*gaussrand());
      //s+= rands[i];
      //printf("%.2f ", rands[i]);
    }
    
*/
int randomgap(){
    FILE *myFile;
    myFile = fopen("randomgap.txt", "r");

    int i;
    if (myFile == NULL){
        printf("Error Reading File\n");
        exit (0);
    }

    for (i = 1; i <= 300; i++){
        fscanf(myFile, "%lf,", &rands[i] );
    }

    /*for (i = 1; i <= 500; i++){
        printf("%d %d\n", i, rands[i]);
    }*/

    fclose(myFile);

    return 0;
}
/*
double gaussrand()
{
	static double V1, V2, S;
	static int phase = 0;
	double X;

	if(phase == 0) {
		do {
			double U1 = (double)rand() / RAND_MAX;
			double U2 = (double)rand() / RAND_MAX;

			V1 = 2 * U1 - 1;
			V2 = 2 * U2 - 1;
			S = V1 * V1 + V2 * V2;
			} while(S >= 1 || S == 0);

		X = V1 * sqrt(-2 * log(S) / S);
	} else
		X = V2 * sqrt(-2 * log(S) / S);

	phase = 1 - phase;

	return X;
}
*/
int recv_ctrlmess()
{
	int i, sockfd;
	char ctrlmess[20];
    struct sockaddr_in my_addr, cli_addr;
    socklen_t slen;
	char buf[BUFLEN];
	slen=sizeof(cli_addr);
	if((sockfd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))==-1)
	      err("socket");
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT_);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

#ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
        perror("setsockopt(SO_REUSEPORT) failed");
#endif

    if (bind(sockfd, (struct sockaddr* ) &my_addr, sizeof(my_addr))==-1)
    	      err("bind");
    struct timeval timeout, t1, t2;
    printf("Recv ctrl mess\n");
   // while(1){
    	i=recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&cli_addr, &slen);
    	gettimeofday(&t1, NULL);
    	sscanf(buf, "%s", ctrlmess);
    	//if(i<400) break;
    //}
    printf("Ctrl mess %s\n", ctrlmess);
    if (strcmp(ctrlmess,"S") ==0)
	return 1;
    else return 0;
}

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
    
    long time[100002];  // max 100000 packets
    if (number >= 100002){
        number = 100000;
        printf("increase time array size!\n");
    }
    gettimeofday(&t0, NULL);
    for( i=1;i<=number;i++)
    {
    	//duration = //rands[i];
    	printf("%d %.2f ", i, duration);
        gettimeofday(&t1,NULL);
        //sprintf(buf, "%d, %ld, %ld\n", i, t1.tv_sec, t1.tv_usec);
        time[i]=(t1.tv_sec-t0.tv_sec)*1000000+(t1.tv_usec-t0.tv_usec);
	sprintf(buf, "%d, %ld\n", i, time[i]);
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
   for (i=2;i<=number;i++){
      // if ((time[i]-time[i-1])>2*duration)
     //       printf("sending interval: %ld us\n", time[i]-time[i-1]);
	sndtimestamp[i] = sndtimestamp[i-1] + time[i] - time[i-1];
     //   printf("sndtimestamp [%d] = %ld us\n", i, sndtimestamp[i] );
   }

}

int main(int argc, char* argv[])
{
    int num=500;//default 500 packets
    int rate=100; // default 100 Mbps
    double gap;
    int pktsize=1472;
    char *serverIP=argv[1];
    if(argc>1) num=atoi(argv[2]);
    if(argc>2) rate=atoi(argv[3]);
    gap = (pktsize+28)*8/rate;
    //srand(time(0));
	//randomgap();
    printf("send %d pkts to %s at rate %i Mbps (interval %.1f us) ", num, serverIP, rate, gap);
    int ctrl = recv_ctrlmess();
    if(ctrl == 1){
	printf("%d send trains\n", ctrl);
    	send_trains(serverIP,  num, pktsize, gap); //sending process
    }
    return 0;
}

