//newest version before tcp
#include<stdlib.h>
#include <stdio.h>
#include<sys/socket.h>
#include<netinet/ip.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/net_tstamp.h>
#include<netinet/ip_icmp.h>
#include<net/ethernet.h>
#include<arpa/inet.h>
#include<string.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#define BUFLEN 1500
#define PORT1 9001
#define VM 0// emulate vm scheduling with N0 pkts and T0 if 1, otherwise normal
#define N0 300
//#define T0 1000 //in usec
#define Lower 100 //in usec
#define Upper 20000 //in usec: vm_time
//#define G 50 // num of groups in PCT or PDT => 50pkts/group
#define PORT_ 9945 // to send ctrl message
#define N1 20 // number of received packets before sending ctrl mess to snd1
#define NUM_PKT 100000


double gaptrend_, gapthres=0.05;

void err(char *s)
{
    perror(s);
    exit(1);
}

double PCT(double array[], int start, int end)
{
	int i, improvement =0;
	double total;
	for (i=start; i<end-1; i++){
		if ( array[i] < array[i+1] )
		        improvement += 1 ;
	}
	total = ( end - start ) ;
	return ( (double)improvement/total ) ;
}
double DEC_PCT(double array[], int start, int end)
{
	int i, improvement =0;
	double total;
	for (i=start; i<end-1; i++){
		if ( array[i] > array[i+1] )
		        improvement += 1 ;
	}
	total = ( end - start ) ;
	return ( (double)improvement/total ) ;
}

double PDT(double array[], int start, int end)
{
	double y = 0 , y_abs = 0 ;
	int i ;
	for ( i = start+1 ; i < end    ; i++ )
	    {
	      y += array[i] - array[i-1] ;
	      y_abs += fabs(array[i] - array[i-1]) ;
	    }
	return y/y_abs ;
}
double median(int n, long x[])
{
	double temp;
	int i, j;
	for(i=0; i<n-1; i++){
		for(j=i+1; j<n; j++){
			if(x[j]<x[i]){
				temp = x[i];
				x[i] = x[j];
				x[j] = temp;
			}
		}
	}
	if(n%2==0){
		return ((x[n/2]+x[n/2 - 1])/2.0);
	}
	else{
		return (x[n/2]);
	}
}
void send_ctrlmess(char *serverIP, char *ctrmess)
{
	//char s[80];
    struct sockaddr_in serv_addr;
    int sockfd, slen=sizeof(serv_addr);
    char buf[BUFLEN];
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
        err("socket");
    int timestamp_flags=SOF_TIMESTAMPING_TX_SOFTWARE;
    if(setsockopt(sockfd, SOL_SOCKET,SO_TIMESTAMPING,&timestamp_flags,sizeof(timestamp_flags))<0) {
        err("timestamp error");
    }
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT_);
    if (inet_aton(serverIP, &serv_addr.sin_addr)==0){
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
	sprintf(buf, "%s\n", ctrmess);
    if (sendto(sockfd, buf, 300, 0,   (struct sockaddr*)&serv_addr, slen)==-1)
                err("sendto()");
    close(sockfd);
}

/*argv[1]=snd1IP, argv[2]=rate1, argv[3]=rate2*/

int main(int argc, char *argv[])
{
	int i,j=0;
	srand(time(0));
    long temp1, temp2;
	char * serverIP;
	struct sockaddr_in my_addr, cli_addr;
    int sockfd;
    double r;
    socklen_t slen;
	char buf[2000];
	long t1=0, t2;
	struct timeval   curr;
	slen=sizeof(cli_addr);
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
	      err("socket");
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT1);
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
    int pktsize;
    int first=0, first_mm[50];
    int nexti, nextnexti;
   // j=0;
    struct timeval microsec [NUM_PKT];
    int id[NUM_PKT];
    long rtt[NUM_PKT];
    int negpos[NUM_PKT];
    long sndus[NUM_PKT], act_rcvtime[NUM_PKT], sndus1[NUM_PKT];

    long act_rcvgap[NUM_PKT], act_sndgap[NUM_PKT];

    long max_diff=0;
    int pktnum=0, count=0, jj=0, ii=0, tt=0, qq=0, N0count=0;

    bzero(negpos,sizeof(negpos));
    int k=0;

	int slot_size[NUM_PKT], slotid=0, totpkt_slot=0, slot_pos[NUM_PKT], sid=0, positive_pos[NUM_PKT];

	while(1) {
        i=recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&cli_addr, &slen);
        if(i<300) break;

      //  if(j<3) printf("%d\n", i);
        gettimeofday(&microsec[pktnum], NULL);
        sscanf(buf, "%d, %ld\n", &id[pktnum], &sndus[pktnum]);
	printf("%d id=%d sndtime=%ld actrcvtime=%ld\n", pktnum, id[pktnum], sndus[pktnum], microsec[pktnum].tv_sec*1000000 + microsec[pktnum].tv_usec);
	//printf("sndtime %d = %ld\n", id[pktnum], sndus[pktnum]);
	//if (id[pktnum]>0)
	//	printf("recv pos_pkt[%d] with id=%d rcvtime: %ld s %ld us\n", pktnum, id[pktnum], microsec[pktnum].tv_sec, microsec[pktnum].tv_usec);
        //rtt[j]=(microsec[j].tv_sec-temp1)*1000000+microsec[j].tv_usec-temp2;
	//printf("id=%d\n", id[pktnum]);
      // rtt[pktnum] = (microsec[pktnum].tv_sec-microsec[pktnum-1].tv_sec)*1000000+microsec[pktnum].tv_usec-microsec[pktnum-1].tv_usec;
        if(id[pktnum]<0){

		if(pktnum <N1 && abs(id[pktnum]) > N1){
			continue;
		}
        	negpos[jj] = pktnum;
        	//printf("negpos[%d] =%d sndtime=%ld\n", jj, negpos[jj], sndus[pktnum]);
        	jj++;
		/*char *serverIP=argv[1];
		printf("send ctrl mess to server %s\n", serverIP);
		send_ctrlmess(serverIP, "S");*/

	}
        else if (id[pktnum]>0){
		if(pktnum < N1){
			continue;
		}
		positive_pos[count] = pktnum;
        	sndus1[count]= sndus[pktnum];
        	act_rcvtime[count] = microsec[pktnum].tv_sec*1000000 + microsec[pktnum].tv_usec ;
		if(count>0){
			//act_rcvgap[count] = act_rcvtime[count]-act_rcvtime[count-1];
			act_sndgap[count] = sndus1[count]-sndus1[count-1];
			act_rcvgap[count] = act_sndgap[count];
		}
		//printf("%d ", count);
        	count++;
        }
	for(k=2; k<count; k++){
		if(act_sndgap[k] > 5* act_sndgap[k-1]){
			printf("Context Switch at Sender\n");
			exit(0);
		}
	}

        pktnum++;
	N0count++;
	if(pktnum == N1)
	{
		char *serverIP=argv[1];
		printf("send ctrl mess to server %s\n", serverIP);
		send_ctrlmess(serverIP, "S");
	}
	if((VM==1) && (N0count == N0)){
		int T0 = (rand() % (Upper-Lower+1))+Lower;
		printf("Start simulating vm scheduling %d\n", T0);
		usleep(T0-10); // since it can sleep additionally up to 40s, e.g. sleep 1000 takes 970us to 1040us
		N0count = 0;
		printf("Done vm scheduling\n");
	}

       }
	printf("count=%d jj=%d\n", count, jj);
	 double rel_rcvtime[count], rel_rcvtime_prev[count], rel_rcvtime_linearline[count], rel_rcvtime_simple[count], actrcv_timestamp[count];
	 actrcv_timestamp[0] = 0;
	 long relrcv_timestamp[count], relrcv_timestamp_prev[count], relrcv_timestamp_linearline[count], relrcv_timestamp_simple[count], tmp1, tmp2, tmp3;
	 long relrcv_timestamp_sim[count];
	 double act_rel_owd[count], rel_owd[count], rel_recvgap[count];
	 int posnum =0, pp=0;
	 int pospkts, pos_k1, pos_k2, count_;
	//while(count_<= count){
	 for(k=0; k<jj; k++){
		 posnum = negpos[k+1] - negpos[k] -1;
		 pospkts = posnum;
		 pos_k1 = negpos[k+1];
		 pos_k2 = negpos[k];
		 tmp1 = sndus[pos_k1];
		 tmp2 = sndus[pos_k2];
		 //printf("id=%d sndus=%ld	", id[k], sndus[k]);
		 if(pospkts > 0){
				slot_size[slotid] = pospkts;
				slot_pos[slotid] = negpos[k];
				totpkt_slot+= slot_size[slotid];
				slot_pos[slotid+1] = negpos[k+1];
				printf("slotid=%d slot_pos=%d ptks=%d\n", slotid, slot_pos[slotid], slot_size[slotid]);
				slotid++;
		 }
	 }

	 for(k=0; k<count; k++){
		for(sid=0; sid<slotid; sid++){
			if(positive_pos[k] > slot_pos[sid] && positive_pos[k] < slot_pos[sid+1]){
				break;
			}
		}
		if(sid==slotid){
			printf("There's a bug. Please increase the number of clock packets\n");
			exit(0);
		}
		tmp2 = sndus[slot_pos[sid]];
		tmp1 = sndus[slot_pos[sid+1]];
		rel_rcvtime[k] = tmp2 + (positive_pos[k] - slot_pos[sid])*(tmp1 - tmp2)/(double)slot_size[sid];
		printf("%d sid=%d positive_pos=%d tmp2=%ld tmp1=%ld relrcvtime=%.2f\n", k, sid, positive_pos[k], tmp2, tmp1, rel_rcvtime[k]);

	 }


	 int alpha = 1; //atoi(argv[2])/(atoi(argv[3]));
	 int checkalpha =0;
	 int firstalpha=0;
	 if(slotid<1){
		printf("Clock rate is too slow\n");
		return;
	 }
	 if(count < 300*0.99){
		printf("Too many packets lost of the train from sender\n");
		return;
	 }

		double rel_rcvgap[count], rel_actsndgap[count];


	 for(k=1; k<count; k++){
		//relrcv_timestamp[k] = relrcv_timestamp[k-1] + rel_rcvtime[k] - rel_rcvtime[k-1];
//		relrcv_timestamp[k] = rel_rcvtime[k] - rel_rcvtime[firstalpha];
//		relrcv_timestamp_prev[k] = rel_rcvtime_prev[k] - rel_rcvtime_prev[firstalpha];
//		relrcv_timestamp_simple[k] = rel_rcvtime_simple[k] - rel_rcvtime_simple[firstalpha];
		//if(k>firstalpha) relrcv_timestamp_sim[k] = rel_rcvtime_simple[k] - rel_rcvtime_simple[firstalpha];
//		sndus1[k] = sndus1[k] -  sndus1[firstalpha];

		/*for(sid=0; sid<slotid; sid++){
			if(positive_pos[k] > slot_pos[sid] && positive_pos[k] < slot_pos[sid+1]){
				break;
			}
		}*/
	
		/*if(sid == 0){
			rel_rcvgap[k] = (double) (rel_rcvtime[slot_size[0]+1] - rel_rcvtime[slot_size[0]]);
			//unsmoothed_gap_array[0] = rel_rcvgap[k];
		}
		else if(sid >=slotid-1 ){
			rel_rcvgap [k] = (double) (rel_rcvtime[count-slot_size[slotid-1] -1] - rel_rcvtime[count-slot_size[slotid-1] -2]);
			//unsmoothed_gap_array[sid] = rel_rcvgap[k];
			}
		else{*/
			rel_rcvgap[k] = (double) (rel_rcvtime[k] - rel_rcvtime[k-1]);
			//unsmoothed_gap_array[sid] = rel_rcvgap[k];
		       // rel_rcvgap[k] = (rel_rcvgap[k]+unsmoothed_gap_array[sid-1])/2.0;
		//}


			//rel_rcvgap[k] = relrcv_timestamp[k] - relrcv_timestamp[k-1];
//			rel_actsndgap[k] = (double) sndus1[k] - (double) sndus1[k-1];

		//actrcv_timestamp[k] = act_rcvtime[k] - act_rcvtime[firstalpha];
		//printf("%d %ld %ld %ld %ld \n", k, actrcv_timestamp[k], relrcv_timestamp_simple[k], relrcv_timestamp_prev[k], relrcv_timestamp[k]);
		
//    	act_rel_owd[k] = (actrcv_timestamp[k]-sndus1[k]); // owd in microsecond
//	rel_owd[k] = (relrcv_timestamp[k]-sndus1[k]); // owd in microsecond

    	//printf("%d act_rel_owd=%ld, rel_owd = %ld, rel_timestamp=%ld, sndus1 =%ld \n", k, act_rel_owd[k], rel_owd[k], relrcv_timestamp[k], sndus1[k] );
	//printf("%d %ld %ld %ld %ld %ld\n", k, act_recvgap[k], act_rel_owd[k], rel_recvgap[k], rel_owd[k], relrcv_timestamp[k]);
       }

// just to smooth the first two slots
/*	 for(k=1; k<count; k++){

		for(sid=0; sid<slotid; sid++){
			if(positive_pos[k] > slot_pos[sid] && positive_pos[k] < slot_pos[sid+1]){
				break;
			}
		}
	
		if(sid >= 2){
			break;
		}
	      //  rel_rcvgap[k] = (rel_rcvgap[k]+unsmoothed_gap_array[2])/2.0;

	}
*/
	/*Compute avg error in timestamps*/
	printf("error percentage, recv_gap\n");
	double errper_simple[count], errper_next[count];
	double errtot_simple = 0.0, errtot_next = 0.0;
	double avgerr_simple=0., agverr_next=0., avgerr_simple2=0.;
	double tot_rel=0, tot_act=0, errtot_simple2;
	for(k=1; k<count; k++){
		tot_rel = tot_rel+rel_rcvgap[k];
		tot_act = tot_act+act_rcvgap[k];
//		errper_simple[k] = fabs(rel_rcvgap[k]/((double)act_rcvgap[k]) -1);
		errper_simple[k] = rel_rcvgap[k]/((double)act_rcvgap[k]) -1;

//		errper_next[k] = (double) abs( (relrcv_timestamp[k] - actrcv_timestamp[k])) / (actrcv_timestamp[k]);
		//printf("%d %.4lf %.4lf\n", k, errper_simple[k], errper_next[k]);

		errtot_simple = errtot_simple + errper_simple[k];
		errtot_simple2 = errtot_simple2+ fabs( errper_simple[k]);

		printf("%d relrcvgap=%.1f actrcvgap=%ld err=%.4f err_tot=%.4f\n", k, rel_rcvgap[k], act_rcvgap[k], errper_simple[k], errtot_simple);
//		errtot_prev = errtot_prev + errper_prev[k];
//		errtot_next = errtot_next + errper_next[k];
		//printf("%d %ld %ld %ld %ld \n", k, actrcv_timestamp[k], relrcv_timestamp_simple[k], relrcv_timestamp_prev[k], relrcv_timestamp[k]);
//		act_recvgap[k] = actrcv_timestamp[k]-actrcv_timestamp[k-1];
//		rel_recvgap[k] = relrcv_timestamp[k]-relrcv_timestamp[k-1];
		//printf("%d %ld %ld %ld %ld %ld\n", k, act_recvgap[k], act_rel_owd[k], rel_recvgap[k], rel_owd[k], relrcv_timestamp[k]);
	}
	avgerr_simple = errtot_simple/(count-1); //(count -1-10);
	avgerr_simple2 = errtot_simple2/(count-1);
	//avgerr_prev = errtot_prev/(k-1-firstalpha); //(count -1-10);
	//agverr_next = errtot_next/(k-1-firstalpha); //(count -1-10);
	printf("%d avgerrtot_simple2=%.4f avgerr_simple=%.4f tot_rel/tot_act=%.4f\n", count-1, avgerr_simple2, avgerr_simple, tot_rel/tot_act);
	/*Pairwise Comparison Test, Pairwise Difference Test*/
/*	int G = 10;//sqrt(k-1-firstalpha); //alpha;
	double owd_median[G], act_owd_median[G];
	int order_med=0, pct_i=0;
	int pkts_pct = (k-1-firstalpha)/G; //(count-1)/G;
	long group_pct[pkts_pct], act_group_pct[pkts_pct];;
*/
/*	while (pct_i<count-1){
		for(k=firstalpha+1; k<pkts_pct; k++){
				group_pct[k] = rel_owd[pct_i];//rel_owd_simple[pct_i];
				act_group_pct[k] = act_rel_owd[pct_i];
				pct_i++;
				//printf("%d group_pct=%ld\n", k, group_pct[k]);
		}
		owd_median[order_med] = median(pkts_pct, group_pct);
		act_owd_median[order_med] = median(pkts_pct, act_group_pct);
		//printf("%d owd_median =%.2f\n", order_med, owd_median[order_med]);
		order_med++;
	}
	double pct_trend = PCT(owd_median, 0, order_med);
	double act_pct_trend = PCT(act_owd_median, 0, order_med);
	//printf("PCT_Trend = %.2lf act_PCT_Trend = %.2lf\n", pct_trend, act_pct_trend);
	double dec_pct = DEC_PCT(owd_median, 0, order_med);
	double dec_act_pct = DEC_PCT(act_owd_median, 0, order_med);
	printf("PCT_Trend = %.2lf act_PCT_Trend = %.2lf, dec_pct=%.2f act_dec_pct=%.2f\n", pct_trend, act_pct_trend, dec_pct, dec_act_pct);
*/
	/*
	double pdt_trend = PDT(owd_median, 0, order_med);
	double act_pdt_trend = PDT(act_owd_median, 0, order_med);
	printf("PDT_Trend = %.2lf act_PDT_Trend = %.2lf\n", pdt_trend,act_pdt_trend);
	*/
	//Not use median
/*	double pct_, pdt_, s=0, a = rel_owd[count-2] - rel_owd[firstalpha+1], a_ = act_rel_owd[count-2] - act_rel_owd[firstalpha+1], act_pct_, dec_pct_, dec_act_pct_, equal;
	double gaptot=0, maxgapthres=1;

	for(k=firstalpha+2; k< count-1; k++){
		s+= abs(rel_owd[k] - rel_owd[k-1]);
		//printf("%d %.2f\n", k, pct_);
		if(rel_owd[k] > rel_owd[k-1]){
			pct_++; act_pct_++;
			//printf("%d %.2f\n", k, pct_);
		}
		else if(rel_owd[k] < rel_owd[k-1]){
			dec_pct_++; dec_act_pct_++;
		}
		else equal++;
		//printf("####gaptrend: relgap=%.2f sndgap=%.1f gaptot=%.2f\n", rel_rcvgap[k], rel_actsndgap[k], gaptot);
			if( (double) rel_rcvgap[k]> (double) ((maxgapthres-gapthres)*rel_actsndgap[k]) && (double) (rel_rcvgap[k]<(maxgapthres+gapthres)*rel_actsndgap[k]) ){
				gaptot++;

			}
	}
	double pct_trend_ = pct_/(count-1-firstalpha-2); double act_pct_trend_ = act_pct_/(count-1-firstalpha-2);
	double dec_pct_trend_ = dec_pct_/(count-1-firstalpha-2); double dec_act_pct_trend_ = dec_act_pct_/(count-1-firstalpha-2);
	double pdt_trend_ = a/s;

		gaptrend_ = gaptot/(count-1-firstalpha-2);
		printf("gaptrend=%.2f, total=%d\n", gaptrend_, count-1-firstalpha-2);
//	printf("Not use median: k =%d total=%d total_pct=%.2f pct=%.2f, pdt=%.2f, act_pct_=%.2f\n", k, count-1-firstalpha-1, pct_, pct_trend_, pdt_trend_, act_pct_trend_);
//	printf("Dec_trend: total_dec_pct=%.2f dec_pct_=%.2f dec_act_pct_=%.2f, Equal=%.2f\n", dec_pct_, dec_pct_trend_, dec_act_pct_trend_, equal);
*/

	 close(sockfd);
return 0;
}

