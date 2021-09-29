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
#include <unistd.h>
#include <sys/time.h>
#define INPUT_PIN 0
#define GPIO_BUTTON 17
#define SERV_TCP_PORT 8000
#define LOW 0
#define HIGH 1
int sockfd;
int gpiofd;
int gpio_read(int fd)
{
    char buf[1+1]={0};
    int ret=-1;
    lseek(fd, 0, SEEK_SET);
    if(read(fd, buf,1)==1)
    {
        ret=atoi(buf);
    }
    return ret;
}
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
    sprintf(buf,"%d",GPIO_BUTTON);
    size=strlen(buf);
    if(size!=write(fd, buf,size)){
        fprintf(stderr, "Failed to write export gpio!\n");
        close(fd);
        return -1;
    }
    close(fd);
    memset(buf,0,sizeof(buf));
    sprintf(buf,  "/sys/class/gpio/gpio%d/direction", GPIO_BUTTON);
    fd = open(buf, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open gpio direction for writing!\n");
        return -1;
    }
    if (2 != write(fd, "in", 2)){
        fprintf(stderr, "Failed to write dir value!\n");
        close(fd);
        return -1;
    }
    close(fd);
    memset(buf,0,sizeof(buf));
    sprintf(buf,"/sys/class/gpio/gpio%d/value",GPIO_BUTTON);
    fd = open(buf, O_RDONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open gpio value for writing val!\n");
        return -1;
    } 
    printf("gpio init suc!\n");
    return fd;
}
void  gpio_bye(void)
{
#if 1
    char buf[64+1];
    int fd;
    int size;
    if(gpiofd!=-1)
        close(gpiofd);
    memset(buf,0,sizeof(buf));
    sprintf(buf,  "/sys/class/gpio/gpio%d/direction", GPIO_BUTTON);
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
    sprintf(buf,"%d",GPIO_BUTTON);
    size=strlen(buf);
    if(size!=write(fd, buf,size)){
        fprintf(stderr, "Failed to write gpio value for writing unexport!\n");
        close(fd);
    }
    close(fd);
#endif
    return;
}

void go_byebye(void)
{
    printf("go_byebye...\n");
    close(sockfd);
    gpio_bye();
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
int chk_press(void)
{

    static time_t pr_sec=0;
    static int pre_dig=0;
    static int press=0;
    int now_dig;
    int ret=0;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    now_dig=gpio_read(gpiofd);
    if(now_dig<0)
        ret=-1;
    if(pr_sec==tv.tv_sec)
    {
        if(pre_dig==LOW&&pre_dig!=now_dig)
        {
            press++;
            printf("Button is pressed+ in last 1s, now %d\n",press);
        }
    }
    else
    {
        char tmp[64+1]={0};
        int len;
        if(press==1)
        {
            strcpy(tmp,"on");
            printf("send [%s]\n",tmp);
            len=strlen(tmp);
            if(write(sockfd,tmp,len)!=len)
                ret=-1;
        }
        if(press>1)
        {
            strcpy(tmp,"off");
            printf("send [%s]\n",tmp);
            len=strlen(tmp);
            if(write(sockfd,tmp,len)!=len)
                ret=-1;
        }
        press=0;
    }
    pr_sec=tv.tv_sec;
    pre_dig=now_dig;
    return ret;
}

int main(int argc, char** argv)
{

    struct sockaddr_in serv_addr;
    char *serv_host = "localhost";
    struct hostent *host_ptr;
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
    printf("is connected!!!\n");
    
    gpiofd=gpio_init();
    signal(SIGPIPE, SIG_IGN);
    signal(SIGPIPE, handle_wrclose);
    signal(SIGINT, handle_intrp);
    /* write a message to the server */
    for(;;)
    {
        if(-1==chk_press())
            go_byebye();
#if 0 //for test       
            int len;
            char tmp[64]={0};
            printf("input message u need send\n");
            scanf("%s",tmp);
            len=strlen(tmp);
            write(sockfd,tmp,len);
#endif
    }
    return 0;

}

