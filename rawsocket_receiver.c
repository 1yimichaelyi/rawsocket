//##########################################################################################
// Raw Socket Test
//##########################################################################################
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/sockios.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h> 
#include <pthread.h>
#include <signal.h>

#include <net/if_arp.h>
#include "queue.h"
#define SIZE_ETHHDR 14  //sizeof(struct ethhdr)
struct raw_result 
{
    unsigned long long bytes;
    long useconds;
    int  packet_sizs;
};
unsigned char mac_reciver[6];
unsigned char mac_sender[6];
char *ifname;
char *peer_ip;
int sock_r;
int eth_MTU = 0;
int Running = 0;
int duration = 0;
struct raw_result tx_result;
struct raw_result rx_result;
int sendback_rx_report = 0;
char cpu_id[20];
char rx_cpu_usage[128];
queue_t *rxQueue = NULL;

//************************************************************************************
// getMTU
//************************************************************************************
int getMTU(char *name) 
{
    FILE *f;
    char buf[128];
    char *line = NULL;
    ssize_t count;
    size_t len = 0;
    int mtu;

    snprintf(buf, sizeof(buf), "/sys/class/net/%s/mtu", name);
    f = fopen(buf, "r");
    if(!f) 
    {
        perror("Error opening:");
        exit(EXIT_FAILURE);
    }
    count = getline(&line, &len, f);

    if(count == -1) 
    {
        perror("Error opening:");
        exit(EXIT_FAILURE);
    }
    sscanf(line, "%d\n", &mtu);
    fclose(f);

    return mtu;
}

//************************************************************************************
// get local mac
//************************************************************************************
int get_local_mac(unsigned char *m)
{
    int fd;
    struct ifreq ifr;
    unsigned char *mac;
 
    fd = socket(AF_INET, SOCK_DGRAM, 0);
 
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name , ifname , IFNAMSIZ-1);
 
    ioctl(fd, SIOCGIFHWADDR, &ifr); 
    close(fd);
 
    mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    memcpy(m, mac, 6);

    return 0;
}
//************************************************************************************
// main
//************************************************************************************
int main(int argc, char* argv[]) 
{
    struct sockaddr_in addr, source, dest;
    int saddr_len = sizeof(addr);
    struct timespec start_time, end_time;
	long total_number_bytes = 0;
	long numbytes;
	double data_interval;
    double estimated_bandwidth;
	unsigned long long serno = 1;
	float percentage;
    unsigned long long total_receive=0;
   
    if (argc < 2){
        printf("usage :  %s [ifname]\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    ifname = argv[1];
    printf("\n");
    //
    // get local mac (reviver)
    //
    get_local_mac(mac_reciver);
    printf("mac_reciver : %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n" , mac_reciver[0], mac_reciver[1], mac_reciver[2], mac_reciver[3], mac_reciver[4], mac_reciver[5]);
    //
    // get MTU
    //
    eth_MTU = getMTU(ifname);
    printf("%s  MTU : %d\n", ifname, eth_MTU);
    printf("\n");
    //
    // create socket for receiving all ethernet packets
    //
    sock_r = socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL)); 
    if(sock_r < 0){
        perror("error in socket\n");
        return -1;
    }
    // create buffer to receive data
    unsigned char *buffer = (unsigned char *) malloc(65536); 
    memset(buffer, 0, 65536);
    // Receive network packet and copy in to buffer

    unsigned char *recv_data = buffer + SIZE_ETHHDR;
    rx_result.bytes = 0;
    int is_got_1st_pak = 0;
    while(1)   
    {
        int numbytes = read(sock_r, buffer, 65536);
        if(numbytes < 0){
            perror("error in reading recvfrom function\n");
            return -1;
        }
        else{
            //
            // only check paks for our mac address
            //
            struct ethhdr *eth = (struct ethhdr *)(buffer);
            if(memcmp(eth->h_source, mac_reciver, 6) == 0){
                // check pak id
                //
                if(strncmp(recv_data, "ing", strlen("ing")) == 0){
					//printf("recv = %s, len = %d\n", recv_data, numbytes);

                    rx_result.bytes += numbytes;
					total_number_bytes += numbytes;
					serno++;
                }
                else if(strncmp(recv_data, "end", strlen("end")) == 0){
				
					char *s = strtok(recv_data, "-");
					char *s1 = strtok(NULL, "-");
					duration=atoi(s1);
					char *s2 = strtok(NULL, "-");
					total_receive=atol(s2);
                    break; 
                }

            }
        }    
    }
	estimated_bandwidth = ((total_number_bytes / 1000000.0) * 8)
                             / duration;
							 
	fprintf(stderr, "received %zd bytes in %d seconds. estimated-"
                "bandwidth %.2f Mbit/s\n", total_number_bytes,duration,
                estimated_bandwidth);
				
	percentage = (float)(total_receive-serno) / total_receive * 100.0;
	printf("\npacket loss percentage = %.2f%%\n", percentage);
	free(buffer);
} 





