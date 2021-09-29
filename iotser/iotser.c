#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "eloop.h"
#define GPIO_ALARM 17
#define SERV_TCP_PORT 8000
struct _clockstat  
{
	int skfd;
    int cnet;
    int gpiofd;
	struct tm tinfo;
	pthread_t alarm;
};
typedef struct _clockstat clockstat;
clockstat gclock;

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
void cnn_bye(void)
{
	close(gclock.skfd);
	close(gclock.cnet);
}

void handle_intrp(int sig)
{
    printf("%s[%d]: signal = %d\n", __FUNCTION__, __LINE__, sig);
	eloop_destroy();
	cnn_bye();
	gpio_bye();
	exit(1);
}

void drop_cnn(int* cnnfd)
{

        eloop_unregister_read_sock(*cnnfd);
		close(*cnnfd);
		*cnnfd=-1;
}

void* tri_alarm(void *data)
{
	int fd=*((int*)data);
	while(1)
    {
		write(fd,"0",1);
		usleep(100*1000);
		write(fd,"1",1);
		usleep(100*1000);
	}
}

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

int cfg_ser(clockstat *p,char *inp[])
{
	if(strcmp(inp[1],"-c")==0
		&&strlen(inp[2])==5
	)
	{
		sscanf(inp[2],"%d:%d",&p->tinfo.tm_hour,&p->tinfo.tm_min);
		printf("set clock %02d:%02d\n",p->tinfo.tm_hour,p->tinfo.tm_min);
		return 1;

	}
	if(strcmp(inp[1],"-a")==0)
	{
		printf("set single alarm mode\n");
		return 1;
	}
	perror("intput parmater error!\n");	
	return -1;
}

int main(int argc, char *argv[])
{

	clockstat* p=&gclock;
	int ret=0;
	memset(&gclock,0,sizeof(gclock));
	struct sockaddr_in serv_addr;

	if(argc<2)
	{
        perror("intput parmater error!\n");
        ret=EXIT_FAILURE;
		goto out;
	}

	if(cfg_ser(p,argv)==-1)
	{
        ret=EXIT_FAILURE;
		goto out;

	}

	p->cnet=-1;
	p->gpiofd=gpio_init();

	if(p->gpiofd<0)
		return EXIT_FAILURE;

	if((p->skfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
        perror("can't open stream socket\n");
        ret=EXIT_FAILURE;
		goto out;

	}

	signal(SIGINT, handle_intrp);
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
 // printf("hoh\n");
	listen(p->skfd, 5);
	//clilen = sizeof(cli_addr);
	//p->cnet = accept(p->skfd, (struct sockaddr *) &cli_addr, &clilen);
	//printf("hoh acept cnn\n");

	eloop_init(NULL);
	eloop_register_timeout(1,0,chk_clockstat,p,NULL);
	eloop_register_read_sock(p->skfd, ser_event_recv,p, NULL);
	eloop_run();

	out:
		eloop_destroy();
		cnn_bye();
		gpio_bye();
	return ret;

}


