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
// Timer
//************************************************************************************
void *Timer()
{
    for(int i=0; i<duration; i++){
        sleep(1);
    }
    Running = 0;
    pthread_exit(0);
}
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
    if(!f){
        perror("Error opening:");
        exit(EXIT_FAILURE);
    }
    count = getline(&line, &len, f);

    if(count == -1){
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
int get_sender_mac(unsigned char *m)
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
// function for sending packet to destination mac
//************************************************************************************
int bind_rawsocket_to_interface(char *device, int rawsock, int protocol)
{
	struct sockaddr_ll sll;
	struct ifreq ifr;

	memset(&sll, 0, sizeof(sll));
	memset(&ifr, 0, sizeof(ifr));
	/* First Get the Interface Index  */
	strncpy((char *)ifr.ifr_name, device, IFNAMSIZ);
	if((ioctl(rawsock, SIOCGIFINDEX, &ifr)) == -1) {
		printf("Error getting Interface index !\n");
		exit(-1);
	}
	/* Bind our raw socket to this interface */
	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = ifr.ifr_ifindex;
	sll.sll_protocol = htons(protocol);
	if((bind(rawsock, (struct sockaddr *)&sll, sizeof(sll)))== -1) {
		perror("Error binding raw socket to interface\n");
		exit(-1);
	}
	return 1;
}

void sender()
{
    int data_len = 0;
    char ch[256];
	char tmp[50];
	int send_len;
    struct timespec start_time, end_time;
    pthread_t tid_Timer;

    int sock_raw = socket(AF_PACKET, SOCK_RAW, ETH_P_ALL);
    if(sock_raw == -1) 
        printf("error in socket");
	
    //allocate send buffer
    unsigned char* sendbuff = malloc(eth_MTU*sizeof(unsigned char));
    //
    memset(sendbuff,'\0', eth_MTU);
        // init eth1
    struct ethhdr *eth1 = (struct ethhdr *)(sendbuff);
    memcpy(eth1->h_source, mac_sender, 6);
    eth1->h_proto = htons(ETH_P_IP);

    bind_rawsocket_to_interface(ifname, sock_raw, ETH_P_ALL);         
    // init tx_result data and start timmer
    data_len = eth_MTU;
    tx_result.bytes = 0;
    tx_result.packet_sizs = data_len - SIZE_ETHHDR;
    Running = 1;
    pthread_create(&tid_Timer, NULL, Timer, NULL); //start timmer
    //
    // loop to send data
    //
    unsigned long long serno = 1;
    while(Running == 1){
        // make serial number
		serno++;
		sprintf(tmp, "ing-%llu", serno);
		strcpy(sendbuff+SIZE_ETHHDR, tmp);

		// send data
		send_len = write(sock_raw, sendbuff, data_len);
		if(send_len < 0){
			perror("sendto");
			exit(-1);
		}
    }
	sprintf(tmp, "end-%d-%lld",duration,serno);
    strcpy(sendbuff+SIZE_ETHHDR, tmp);
			
	send_len = write(sock_raw, sendbuff, data_len);
	if(send_len < 0){
		perror("sendto");
		exit(-1);
	}
	tx_result.bytes += send_len;
	printf("send data ok %d\n",Running);
           
    free(sendbuff);
    close(sock_raw);
}
//************************************************************************************
// main
//************************************************************************************
int main(int argc, char* argv[]) 
{
    struct sockaddr_in addr, source, dest;
    int saddr_len = sizeof(addr);
    struct timespec start_time, end_time;

    if (argc < 2){
        printf("usage :  %s [send to ifname] [test duration-seconds] \n",argv[0]);
        exit(EXIT_FAILURE);
    }
    ifname = argv[1];
    duration = atoi(argv[2]);
    printf("\n");
    // get mac (sender)
    get_sender_mac(mac_sender);
    printf("send to mac : %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n" , mac_sender[0], mac_sender[1], mac_sender[2], mac_sender[3], mac_sender[4], mac_sender[5]);
    // get MTU
    eth_MTU = getMTU(ifname);
    printf("%s  MTU : %d\n", ifname, eth_MTU);
    printf("send mac data to interface(%s) for %d seconds.....\n",ifname,duration);
    sender();
} 
