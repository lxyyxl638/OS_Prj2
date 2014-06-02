#include "client.h"

int main(int argc,char * argv[])
{
    int sockfd;
    int port = atoi(argv[2]);
    sockfd = Socket();
    struct sockaddr_in serv_addr;
    struct hostent *host;
    char buf[MAXSIZE]; 
    int n;

    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    host = gethostbyname(argv[1]);
    memcpy(&serv_addr.sin_addr.s_addr,host->h_addr,host->h_length);
    serv_addr.sin_port = htons(port);

    connect(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr));

    n = Read(sockfd,buf,MAXSIZE);
    Write(STDOUT_FILENO,buf,n);
    while (fgets(buf,MAXSIZE,stdin)!= NULL)
    {
        Write(sockfd,buf,strlen(buf));
        bzero(buf,sizeof(buf));
        n = Read(sockfd,buf,MAXSIZE);
        if (0 == n)
            printf("The other size has been closed\n");
        else
          {
             if (buf[0] != 'c') Write(STDOUT_FILENO,buf,n);
          }
           n = Read(sockfd,buf,MAXSIZE);
           Write(STDOUT_FILENO,buf,n);
    }
    Close(sockfd);
    return 0;
}
