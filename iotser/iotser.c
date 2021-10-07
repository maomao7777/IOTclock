#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <signal.h>
#include "eloop.h"
#define GPIO_ALARM 17
#define SERV_TCP_PORT 8000 /* server's port */
#define SUP_L2_DISC 1
#if SUP_L2_DISC
#define DISC_PROT 0x1357
#define DEF_DISC_IF "eth0"
#define BIND_DISC_SK 1
struct _dispkt
{
	uint8_t da[6];
	uint8_t sa[6];
	uint16_t type;
	char data[46];
};
typedef struct _dispkt dispkt;
#endif
struct _clockstat  
{
	int cnet;
	int gpiofd;
	int skfd;
	struct tm tinfo;
	pthread_t alarm;
#if SUP_L2_DISC
	int disfd;
	char dis_if[16];
#endif
};
typedef struct _clockstat clockstat;
clockstat gclock;
/*-----------------------------------------------------------------------------------------------------------------------*/
int  gpio_init(void)
{
	int fd;
	char buf[64+1];
	int size;
	memset(buf,0,sizeof(buf));
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for writing export!\n");
		return -1;
	}
	sprintf(buf,"%d",GPIO_ALARM);
	size=strlen(buf);
	if(size!=write(fd, buf,size)){
		fprintf(stderr, "Failed to write export gpio!\n");
		close(fd);
		return -1;
	}
	close(fd);
	memset(buf,0,sizeof(buf));
	sprintf(buf,  "/sys/class/gpio/gpio%d/direction", GPIO_ALARM);
	fd = open(buf, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio direction for writing!\n");
		return -1;
	}
	if (3 != write(fd, "out", 3)){
		fprintf(stderr, "Failed to write dir value!\n");
		close(fd);
		return -1;
	}
	close(fd);
	memset(buf,0,sizeof(buf));
	sprintf(buf,"/sys/class/gpio/gpio%d/value",GPIO_ALARM);
	fd = open(buf, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for writing val!\n");
		return -1;
	}
	return fd;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void  gpio_bye(void)
{
	char buf[64+1];
	int fd;
	int size;
	int gpiofd=gclock.gpiofd;
	if(gpiofd)
	{
		if (1 != write(gpiofd, "0", 1)) 
		{
			fprintf(stderr, "Failed to write bye off value!\n");
			close(gpiofd);
		}
	}
	memset(buf,0,sizeof(buf));
	sprintf(buf,  "/sys/class/gpio/gpio%d/direction", GPIO_ALARM);
	fd = open(buf, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio direction for writing!\n");
	}
	if (2 != write(fd, "in", 2)){
		fprintf(stderr, "Failed to write dir bye value!\n");
		close(fd);
	}
	close(fd);
	memset(buf,0,sizeof(buf));
	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for writing unexport!\n");
	}
	sprintf(buf,"%d",GPIO_ALARM);
	size=strlen(buf);
	if(size!=write(fd, buf,size)){
		fprintf(stderr, "Failed to write gpio value for writing unexport!\n");
		close(fd);
	}
	close(fd);
	return;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void cnn_bye(void)
{
	close(gclock.skfd);
	close(gclock.cnet);
#if SUP_L2_DISC
	close(gclock.disfd);
#endif
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void handle_intrp(int sig)
{
	printf("%s[%d]: signal = %d\n", __FUNCTION__, __LINE__, sig);
	eloop_destroy();
	cnn_bye();
	gpio_bye();
	exit(1);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
int if_get_index(const char *interface)
{
	struct ifreq    ifrq;
	int fd;
	/** sanity checks */
	if(interface == NULL)
	{
		printf("%s: interface is NULL!\n", __FUNCTION__);
		return 0;
	}

	/** create socket for ioctls */
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
	{
		memset(&ifrq, 0, sizeof(ifrq));
		strcpy(ifrq.ifr_name, interface);
		if(ioctl(fd, SIOCGIFINDEX, &ifrq) == 0)
		{
			close(fd);
		}
		else
		{
			close(fd);
			return 0;
		}
	}
	else
	{
		return 0;   
	}

	return ifrq.ifr_ifindex;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
const char *if_get_ip(const char *interface)
{
	static char s[16];
	int fd;
	struct ifreq ifr;
	struct sockaddr_in *sin;

	if(interface == NULL)
	{
		printf("discorver interface is NULL!\n");
		return NULL;
	}
	strcpy(ifr.ifr_name, interface);
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
	{
		if(ioctl(fd, SIOCGIFADDR, &ifr) == 0)
		{
			sin = (struct sockaddr_in *) &ifr.ifr_addr;
			snprintf(s, sizeof(s), "%s", (char*)inet_ntoa(sin->sin_addr));
			goto END;
		}
	}
	strcpy(s, "---");
	END:
	close(fd); 
	return s;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
int if_get_hw_addr(const char *interface, u_int8_t *hwaddr)
{
	int sd;
	struct ifreq req;
	int res=1;

	if(interface == NULL)
	{
		printf("%s: interface is NULL!\n", __FUNCTION__);
		res=0;
		return res;
	}
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd == -1)
	{
		printf("open socket4ioctl failed\n");
		res=0;
		goto END;
	}
	sprintf(req.ifr_name,"%s",interface);
	if(ioctl(sd,SIOCGIFHWADDR,&req) == -1)
	{
		res=0;
		goto END;
	}
	memcpy (hwaddr,req.ifr_hwaddr.sa_data,6);
	END:
	close (sd);
	return res;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void drop_cnn(int* cnnfd)
{
	eloop_unregister_read_sock(*cnnfd);
	close(*cnnfd);
	*cnnfd=-1;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void* tri_alarm(void *data)
{
	int fd=*((int*)data);
	time_t pr_sec=0;
	struct timeval tv;
	int period=0;
	while(1)
	{
		gettimeofday(&tv,NULL);
		if(pr_sec!=tv.tv_sec)
		{
			if(period<=330)
				period=660;
			else
				period-=110;
		}
		else
		{
			write(fd,"0",1);
			usleep(period);
			write(fd,"1",1);
			usleep(period);
		}
			
		pr_sec=tv.tv_sec;
	}
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void chk_clockstat(void *eloop_ctx, void *timeout_ctx)
{
	clockstat* p=(clockstat *)eloop_ctx;
	if((p->cnet)>0)
	{
		time_t rawtime;
		struct tm* timeinfo;
		time (&rawtime );
		timeinfo = localtime(&rawtime);
		//printf("now is %02d:%02d\n",timeinfo->tm_hour,timeinfo->tm_min);
		//printf("set is %02d:%02d\n",p->tinfo.tm_hour,p->tinfo.tm_min);
		if(timeinfo->tm_hour==p->tinfo.tm_hour
			&&timeinfo->tm_min==p->tinfo.tm_min
			&&timeinfo->tm_sec==0
			/*time up*/
			&&!p->alarm
		)
		{
#if 1
			pthread_create(&p->alarm, NULL, tri_alarm, &p->gpiofd);
			printf("create tri_alarm pthead is %lu\n",p->alarm);
#endif
		}

	}
	eloop_register_timeout(1,0,chk_clockstat,p,NULL);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void read_event_recv(int socket_fd, void *eloop_ctx, void *timeout_ctx)
{
	int len;
	char string[127+1]={0};
	clockstat* p=(clockstat *)eloop_ctx;
	len = read(p->cnet, string, 127+1); 
	printf("recv len %d\n",len);
	if(len>0)
	{
		string[len]='\0';
		printf("recv buf [%s]\n",string);
		if(strcmp(string,"on")==0)
		{

			if(!p->alarm)
				pthread_create(&p->alarm, NULL, tri_alarm, &p->gpiofd); 
			write(p->cnet,"ok",2);
		}
		else if(strcmp(string,"off")==0)
		{
			if(p->alarm&&
				!pthread_cancel(p->alarm))
			{
					p->alarm=0;
					write(p->gpiofd,"0",1);
			}
			write(p->cnet,"ok",2);
		}
		else if(strcmp(string,"fuck")==0)
		{
			write(p->cnet,"fuck ur mother",32);
		}
		else
			write(p->cnet,"wut u fucking tell me",32);
	}
	else
	{
		printf("drop conn since read < 1\n");
		drop_cnn(&p->cnet);
	}
}
#if SUP_L2_DISC
static unsigned char broadcast_addr[]=
	{0xff,0xff,0xff,0xff,0xff,0xff};
/*
static unsigned char test_addr[]=
	{0xed,0xed,0xed,0xed,0xed,0xed};
*/
/*-----------------------------------------------------------------------------------------------------------------------*/
void dis_event_recv(int socket_fd, void *eloop_ctx, void *timeout_ctx)
{
	printf("hohoh recv dis !!\n");
	dispkt pkt;
	clockstat* p=(clockstat *)eloop_ctx;
	memset(&pkt,0,sizeof(pkt));
#if !BIND_DISC_SK
	struct sockaddr_ll dis_addr;
	size_t len=sizeof(dis_addr);
	memset(&dis_addr, 0,sizeof(dis_addr));
	recvfrom(p->disfd, &pkt, sizeof(pkt), 0,(struct sockaddr *)&dis_addr, &len);
	printf("recv[%s]\n",pkt.data);
	
	dis_addr.sll_family = PF_PACKET;
	dis_addr.sll_protocol =htons(DISC_PROT);
	dis_addr.sll_ifindex=if_get_index(p->dis_if);
	if(strcmp(pkt.data,"fucking tell me seraddr")==0){
		memset(&pkt,0,sizeof(pkt));
		memcpy(pkt.da,broadcast_addr,6);
		if(!if_get_hw_addr(p->dis_if,pkt.sa)){
			printf("can't get hwaddr for l2 discorver\n");
			return;
		}
		pkt.type=htons(DISC_PROT);
		sprintf(pkt.data,"ok::seraddr:%s",if_get_ip(p->dis_if));
		printf("send if_[%s] ipaddr [%s]\n",p->dis_if,if_get_ip(p->dis_if));
		sendto(p->disfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&dis_addr, sizeof(dis_addr));
		printf("send[%d]:[%s]\n",sizeof(pkt),pkt.data);
	}
//reply unknown
/*
	else
	{
		memset(&pkt,0,sizeof(pkt));
		memcpy(pkt.da,broadcast_addr,6);
		if(!if_get_hw_addr(p->dis_if,pkt.sa)){
			printf("can't get hwaddr for l2 discorver\n");
			return;
		}
		pkt.type=htons(DISC_PROT);
		strcpy(pkt.data,"i receive fucking data");
		sendto(p->disfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&dis_addr, sizeof(dis_addr));
	}
*/
#else
	int len;
	len = read(p->disfd,&pkt, sizeof(pkt)); 
	printf("recv len %d\n",len);
	if(len>0)
	{
		printf("recv buf [%s]\n",pkt.data);
	}
	if(strcmp(pkt.data,"fucking tell me seraddr")==0)
	{
		memset(&pkt,0,sizeof(pkt));
		memcpy(pkt.da,broadcast_addr,6);
		if(!if_get_hw_addr(p->dis_if,pkt.sa)){
			printf("can't get hwaddr for l2 discorver\n");
			return;
		}
		pkt.type=htons(DISC_PROT);
		sprintf(pkt.data,"ok::seraddr:%s",if_get_ip(p->dis_if));
		printf("send if_[%s] ipaddr [%s]\n",p->dis_if,if_get_ip(p->dis_if));
		len=write(p->disfd,&pkt,sizeof(pkt));
		printf("send[%d]:[%s]\n",len,pkt.data);
	}
#endif
}
#endif
/*-----------------------------------------------------------------------------------------------------------------------*/
void ser_event_recv(int socket_fd, void *eloop_ctx, void *timeout_ctx)
{
	struct sockaddr_in cli_addr;
	clockstat* p=(clockstat *)eloop_ctx;
	size_t clilen;
	clilen = sizeof(cli_addr);
	printf("Receive con!\n");
	if((p->cnet )>0)
	{
		printf("drop last conn\n");
		drop_cnn(&p->cnet);
	}

	p->cnet = accept(p->skfd, (struct sockaddr *) &cli_addr, &clilen);
	printf("acept new cnn\n");
	eloop_register_read_sock(p->cnet, read_event_recv, (void *)p, NULL);	

}
/*-----------------------------------------------------------------------------------------------------------------------*/
int cfg_serInit(clockstat *p,int ninp,char **inp)
{
	int pra=1;
	int ret=1;
	int setclok=0;
	while(pra<ninp&&
		strlen(inp[pra]))
	{
		if(strcmp(inp[pra],"-c")==0
			&&strlen(inp[pra+1])==5
		)
		{
			sscanf(inp[pra+1],"%d:%d",&p->tinfo.tm_hour,&p->tinfo.tm_min);
			printf("set clock %02d:%02d\n",p->tinfo.tm_hour,p->tinfo.tm_min);
			setclok=1;
		}
		if(strcmp(inp[pra],"-a")==0
			&&setclok!=1)
		{
			printf("set single alarm mode\n");
			setclok=1;
		}
#if SUP_L2_DISC
		if(strcmp(inp[pra],"-d")==0
			&&strlen(inp[pra+1])
		)
		{
			strncpy(p->dis_if,inp[pra+1],sizeof(p->dis_if));
			if(!if_get_index(p->dis_if))
				ret=-1;
			else
				printf("set bind discorver interface %s\n",p->dis_if);
		}
#endif
		pra++;	
	}
	if (setclok==0)
		ret=-1;
#if SUP_L2_DISC
	if(!strlen(p->dis_if))
	{
		strncpy(p->dis_if,DEF_DISC_IF,sizeof(p->dis_if));
		if(!if_get_index(p->dis_if))
			ret=-1;
		else
			printf("set bind discorver interface %s\n",p->dis_if);
	}
#endif
	p->cnet=-1;
	p->gpiofd=gpio_init();
	if(p->gpiofd<0)
		ret=-1;
	return ret;
}
/*-----------------------------------------------------------------------------------------------------------------------*/
int main(int argc, char** argv)
{

	clockstat* p=&gclock;
	int ret=0;
	memset(&gclock,0,sizeof(gclock));
	struct sockaddr_in serv_addr;

	if(argc<2)
	{
		printf("intput parmater incorrect!\n");
		return EXIT_FAILURE;
	}
	if(cfg_serInit(p,argc,argv)==-1)
	{
		printf("set parmater error!\n");
		return EXIT_FAILURE;
	}
	
	signal(SIGINT, handle_intrp);
#if SUP_L2_DISC /*create discorver ser sk .....*/
	if ((p->disfd = socket(PF_PACKET, SOCK_RAW, htons(DISC_PROT))) < 0) 
	{
		perror("can't open discorver listen socket\n");
		ret=EXIT_FAILURE;
		goto out;
	}
#if BIND_DISC_SK
	struct sockaddr_ll dis_addr;
	memset(&dis_addr, 0,sizeof(dis_addr));
	dis_addr.sll_family = PF_PACKET;
	dis_addr.sll_protocol =htons(DISC_PROT);
	dis_addr.sll_ifindex=if_get_index(p->dis_if);
	printf("bind recv discorver ifindex is %d, disfd is %d\n",dis_addr.sll_ifindex,p->disfd);
	if(bind(p->disfd , (struct sockaddr *) &dis_addr, sizeof(dis_addr)) < 0) {
		perror("can't bind discorver listen sockaddr\n");
		ret=EXIT_FAILURE;
		goto out;
	}
#endif
#endif

#if 1/* create server tcp sk............*/
	if((p->skfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
		perror("can't open server socket\n");
		ret=EXIT_FAILURE;
		goto out;

	}

/* bind the local address, so that the cliend can send to server */

	memset(&serv_addr, 0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(SERV_TCP_PORT);

	if(bind(p->skfd , (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("can't bind local address\n");
		ret=EXIT_FAILURE;
		goto out;
	}


/* open a TCP socket (an Internet stream socket) */
/* listen to the socket */
	listen(p->skfd, 5);
#endif
	eloop_init(NULL);
#if SUP_L2_DISC
	printf("reg l2 discorver listen sock\n");
	eloop_register_read_sock(p->disfd, dis_event_recv,p, NULL);
#endif
	eloop_register_read_sock(p->skfd, ser_event_recv,p, NULL);
	eloop_register_timeout(1,0,chk_clockstat,p,NULL);
	eloop_run();
	goto out;
	
	out:
		eloop_destroy();
		cnn_bye();
		gpio_bye();

	return ret;
}
