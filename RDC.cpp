#include "client.h"

pthread_t ntid;

int sockfd,o,oo = 0;
char sendbuf[MAXSIZE],recebuf[MAXSIZE];

void *Receive(void * arg)
{
    int n;
    while (1)
    {   
       bzero(recebuf,sizeof(recebuf));
       n = Read(sockfd,recebuf,MAXSIZE);
       if (0 == n)
         {
           printf("The other size has been closed\n");
           return (void *) 1;
         }
       else
        {
             Write(STDOUT_FILENO,recebuf,n);  
             if (recebuf[0] == 'E') return (void *)1;
        }
    }
}

int main(int argc,char * argv[])
{
    int port = atoi(argv[2]);
    o = atoi(argv[3]);
    int n,err;
    struct sockaddr_in serv_addr;
    struct hostent *host;
    int cylinders,sectors,c,s,Order,i,length;
    char * tmp = new char[MAXSIZE];

    sockfd = Socket();
    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    host = gethostbyname(argv[1]);
    memcpy(&serv_addr.sin_addr.s_addr,host->h_addr,host->h_length);
    serv_addr.sin_port = htons(port);

    connect(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr));

    srand(time(NULL));
    ++o;
    printf("The command amount is %d\n",o);

    bzero(sendbuf,sizeof(sendbuf));
    strcat(sendbuf,"I\n");
    printf("%s",sendbuf);
    Write(sockfd,sendbuf,strlen(sendbuf));
    n = Read(sockfd,sendbuf,MAXSIZE);
    i = geti(sendbuf,cylinders);
    i = geti(&sendbuf[i],sectors);
    printf("%d %d\n",cylinders,sectors);
    
    err = pthread_create(&ntid,NULL,Receive,NULL);
    if (err != 0)
      {
          perror("create thread failed\n");
          exit(1);
      }

    while (o > 1)
    {
        --o;
        Order = rand() % 2;
        if (Order == 0)  // read
        {
            bzero(sendbuf,sizeof(sendbuf));
            strcat(sendbuf,"R ");
            c = rand() % cylinders;
            s = rand() % sectors;
            tmp = itoa(c,tmp);
            strcat(tmp," ");
            strcat(sendbuf,tmp);
            tmp = itoa(s,tmp);
            strcat(tmp,"\n");
            strcat(sendbuf,tmp);
        }
        else
        {
            bzero(sendbuf,sizeof(sendbuf));
            strcat(sendbuf,"W ");
            c = rand() % cylinders;
            s = rand() % sectors;
            tmp = itoa(c,tmp);
            strcat(tmp," ");
            strcat(sendbuf,tmp);
            tmp = itoa(s,tmp);
            strcat(tmp," ");
            strcat(sendbuf,tmp);
            length = rand() % 100 + 1;
            tmp = itoa(length,tmp);
            strcat(tmp," ");
            strcat(sendbuf,tmp);
            for (int i = 0;i < length;++i)
                tmp[i] = rand() % 10 + '0';
            strcat(sendbuf,tmp);
            strcat(sendbuf,"\n");
        }
        printf("%d %s",o,sendbuf);
        Write(sockfd,sendbuf,strlen(sendbuf));
    }
    bzero(sendbuf,sizeof(sendbuf));
    strcat(sendbuf,"EXIT\n");
    Write(sockfd,sendbuf,strlen(sendbuf));
    printf("EXIT\n");
    pthread_join(ntid,NULL);
    Close(sockfd);
    return 0;
}
