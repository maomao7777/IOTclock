#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <sys/time.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h> 
#define INPUT_PIN 0
#define GPIO_BUTTON 17
#define SERV_TCP_PORT 8000
#define LOW 0
#define HIGH 1
#define SUP_L2DIS 0
int sockfd;
#if SUP_L2DIS
int disfd;
#endif
void go_byebye(void)
{
    printf("go_byebye...\n");
    close(sockfd);
    exit(1);
}
void handle_intrp(int sig)
{
    printf("handle_intrp signal[%d], stop remote\n", sig);
    go_byebye();
}
void handle_wrclose(int sig)
{
    printf("handle send_close signal[%d], stop remote\n", sig);
    go_byebye();
}
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
int main(int argc, char** argv)
{

    struct sockaddr_in serv_addr;
    struct sockaddr_ll dis_addr;
    char *serv_host = "localhost";
    struct hostent *host_ptr;
    unsigned char broadcast_addr[]=
    {0xff,0xff,0xff,0xff,0xff,0xff};
    int port;
    printf("argc is %d argv is %s\n",argc,argv[1]);

    /* command line: client [host [port]]*/
    if(argc >= 2) 
        serv_host = argv[1]; /* read the host if provided */
    if(argc == 3)
        sscanf(argv[2], "%d", &port); /* read the port if provided */
    else 
        port = SERV_TCP_PORT;

    /* get the address of the host */

    if((host_ptr = gethostbyname(serv_host)) == NULL) {
        perror("gethostbyname error");
        exit(1);
    }

    if(host_ptr->h_addrtype !=  AF_INET) 
    {
        perror("unknown address type");
        exit(1);
    }
#if SUP_L2DIS
    disfd=-1;
    if((disfd = socket(PF_PACKET, SOCK_RAW, htons(0x1357))) < 0) {

        perror("can't open l2 dis socket");
        exit(1);

    }
    
    memset(&dis_addr, 0,sizeof(dis_addr));
    dis_addr.sll_family = PF_PACKET;
    dis_addr.sll_protocol = htons(0x1357);
    dis_addr.sll_ifindex=if_get_index("enp2s0");
    printf("send ifindex is %d\n",dis_addr.sll_ifindex);
    memcpy(dis_addr.sll_addr,broadcast_addr,6);
#endif
#if 1 
    sockfd=-1;
    memset(&serv_addr,0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = 
    ((struct in_addr *)host_ptr->h_addr_list[0])->s_addr;

    serv_addr.sin_port = htons(port);

    /* open a TCP socket */


    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {

        perror("can't open stream socket");
        exit(1);

    }
    /* connect to the server */ 

    printf("wait for connecting...\n");

    if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("can't connect to server");
        //exit(1);
    }
    else
        printf("is connected!!!\n");
#endif

    signal(SIGPIPE, SIG_IGN);
    signal(SIGPIPE, handle_wrclose);
    signal(SIGINT, handle_intrp);
    /* write a message to the server */
    for(;;)
    {
#if SUP_L2DIS
            dispkt pkt;
			size_t len=sizeof(pkt);
			memset(&pkt,0,len);
			memcpy(pkt.da,broadcast_addr,6);
			//memcpy(pkt.sa,src_addr,6);
			pkt.type=htons(0x1357);
            printf("input data u need send\n");
            scanf("%s",pkt.data);
            sendto(disfd, &pkt, len, 0, (struct sockaddr *)&dis_addr, sizeof(dis_addr));
			len=sizeof(dis_addr);
			recvfrom(disfd, &pkt, sizeof(pkt), 0,(struct sockaddr *)&dis_addr, &len);
       
#else //for test       
            int len;
            char tmp[64]={0};
            printf("input message u need send\n");
            scanf("%s",tmp);
            len=strlen(tmp);
            write(sockfd,tmp,len);
            memset(tmp,0,sizeof(tmp));
            read(sockfd,tmp,64);
            printf("recv [%s]\n",tmp);
#endif
    }
    return 0;

}

