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
#define SUP_L2_DISC 1
int sockfd;
int gpiofd;
#if SUP_L2_DISC
#define DISC_PROT 0x1357
int disfd;
struct _dispkt
{
    uint8_t da[6];
    uint8_t sa[6];
    uint16_t type;
    char data[46];
};
typedef struct _dispkt dispkt;
#endif
/*-----------------------------------------------------------------------------------------------------------------------*/
void go_byebye(void)
{
    printf("go_byebye...\n");
    close(sockfd);
#if SUP_L2_DISC
    close(disfd);
#endif
    exit(1);
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void handle_intrp(int sig)
{
    printf("handle_intrp signal[%d], stop remote\n", sig);
    go_byebye();
}
/*-----------------------------------------------------------------------------------------------------------------------*/
void handle_wrclose(int sig)
{
    printf("handle send_close signal[%d], stop remote\n", sig);
    go_byebye();
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
#if SUP_L2_DISC
static unsigned char broadcast_addr[]=
    {0xff,0xff,0xff,0xff,0xff,0xff};
/*
static unsigned char test_addr[]=
    {0xed,0xed,0xed,0xed,0xed,0xed};
*/
/*-----------------------------------------------------------------------------------------------------------------------*/
int	dis_ser_addr(char* serv_host,char* us_if)
{
    dispkt pkt;
    struct sockaddr_ll dis_addr;
    size_t len;
    disfd=-1;
    if((disfd = socket(PF_PACKET, SOCK_RAW,htons(DISC_PROT))) < 0) 
    {
        perror("can't open l2 dis socket");
        return-1;
    }
    for(;;)
    {
        memset(&dis_addr, 0,sizeof(dis_addr));
        dis_addr.sll_family = PF_PACKET;
        dis_addr.sll_protocol =htons(DISC_PROT);
        dis_addr.sll_ifindex=if_get_index(us_if);
        len=sizeof(pkt);
        memset(&pkt,0,len);
        memcpy(pkt.da,broadcast_addr,6);
        if(!if_get_hw_addr(us_if,pkt.sa))
        {
            printf("can't get hwaddr for l2 discorver\n");
            //need to set pkt.da for ap transverse eth<=>wlan frame
            return-1;
        }
        pkt.type=htons(DISC_PROT);
#if 0 //test input
        printf("input data u need send from if_%d\n",dis_addr.sll_ifindex);
        scanf("%s",pkt.data);
        sendto(disfd, &pkt, len, 0, (struct sockaddr *)&dis_addr, sizeof(dis_addr));
#else
        strcpy(pkt.data,"fucking_tell_me_seraddr");
        printf("disc_send_if[%d]:[%s]\n",dis_addr.sll_ifindex,pkt.data);
        sendto(disfd, &pkt, len, 0, (struct sockaddr *)&dis_addr, sizeof(dis_addr));
        len=sizeof(dis_addr);
        if(recvfrom(disfd, &pkt, sizeof(pkt), 0,(struct sockaddr *)&dis_addr, &len)>0)
        {
            char* p;
            printf("recv[%s]\n",pkt.data);
            p=strstr(pkt.data,"ok::seraddr:");
            if(p!=NULL)
            {
                strcpy(serv_host,p+strlen("ok::seraddr:"));
                printf("discorver seraddr is [%s]\n",serv_host);
                return 1;
            }
        }
#endif
    }
    return-1;
}
#endif
/*-----------------------------------------------------------------------------------------------------------------------*/
int main(int argc, char** argv)
{
    struct sockaddr_in serv_addr;    
    char serv_host[17+1] = "localhost";
    struct hostent *host_ptr;

    int port=SERV_TCP_PORT;

    /* command line: client [host [port]]*/
#if SUP_L2_DISC
    if(argc==3&&
        strcmp("-d",argv[1])==0
        &&strlen(argv[2]))
    {
        printf("set discorver if %s\n",argv[2]);
        if(dis_ser_addr(serv_host,argv[2])<0)
        {
            perror("discorver seraddr error");
            exit(1);
        }
    }
    else
#endif
    if(argc >= 2) 
        strcpy(serv_host,argv[1]); /* read the host if provided */
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
        exit(1);
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
#if 1 //for test       
        int len;
        int sndc;
        char snd[128];
        char tmp[64];
        char tmp2[64];
        memset(tmp,0,sizeof(tmp));
        memset(tmp2,0,sizeof(tmp2));
        printf("input msg u need to send\n");
        sndc=scanf("%s %s",tmp,tmp2);
        if(!sndc){
            printf("scan input error\n");
            continue;
        }
        if(sndc==1)
            sprintf(snd,"%s",tmp);
        else if (sndc==2)
            sprintf(snd,"%s %s",tmp,tmp2);
        len=strlen(snd);
        if(write(sockfd,snd,len)!=len)
            printf("send msg error\n");
        memset(snd,0,sizeof(snd));
        if(read(sockfd,snd,sizeof(snd))>0)
            printf("recv[%s]\n",snd);
#endif
    }
    return 0;
}
 
