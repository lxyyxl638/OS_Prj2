#include "Project2.h"
 
int main(int argc,char * argv[])
{
    int sockfd;
    int port = atoi(argv[2]);
    sockfd = Socket();
    struct sockaddr_in serv_addr;
    struct hostent *host;
    char buf[MAXSIZE]; 
    int n,cylinders,sectors,c,s,Order,i,length;
    char * tmp = new char[MAXSIZE];

    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    host = gethostbyname(argv[1]);
    memcpy(&serv_addr.sin_addr.s_addr,host->h_addr,host->h_length);
    serv_addr.sin_port = htons(port);

    connect(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr));
    
    bzero(buf,sizeof(buf));
    strcat(buf,"I\n");
    printf("%s",buf);
    Write(sockfd,buf,strlen(buf));
    n = Read(sockfd,buf,MAXSIZE);
    i = geti(buf,cylinders);
    printf("%d\n",i);
    i = geti(&buf[i],sectors);
    printf("%d %d\n",cylinders,sectors);
    srand(time(NULL));
    int o = rand() % 100 + 1;
    printf("%d\n",o);
    while (o > 0)
    {
        --n;
        Order = rand() % 2;
        if (Order == 0)  // read
        {
           bzero(buf,sizeof(buf));
           strcat(buf,"R ");
           c = rand() % cylinders;
           s = rand() % sectors;
           tmp = itoa(c,tmp);
           strcat(tmp," ");
           strcat(buf,tmp);
           tmp = itoa(s,tmp);
           strcat(tmp," ");
           strcat(buf,tmp);
        }
        else
        {
           bzero(buf,sizeof(buf));
           strcat(buf,"W ");
           c = rand() % cylinders;
           s = rand() % sectors;
           tmp = itoa(c,tmp);
           strcat(tmp," ");
           strcat(buf,tmp);
           tmp = itoa(s,tmp);
           strcat(tmp," ");
           strcat(buf,tmp);
           length = rand() % 256 + 1;
           tmp = itoa(length,tmp);
           strcat(tmp," ");
           strcat(buf,tmp);
           for (int i = 0;i < length;++i)
             tmp[i] = rand() % 10 + '0';
           tmp[BLOCKSIZE] = 0;
           strcat(buf,tmp);
        }
        Write(STDOUT_FILENO,buf,strlen(buf));
        printf("\n");
        Write(sockfd,buf,strlen(buf));
        bzero(buf,sizeof(buf));
        n = Read(sockfd,buf,MAXSIZE);
        if (0 == n)
            printf("The other size has been closed\n");
        else
            Write(STDOUT_FILENO,buf,n);

    }
    Close(sockfd);
    return 0;
}
