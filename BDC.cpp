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

    while (fgets(buf,MAXSIZE,stdin)!= NULL)
    {
        if (buf[0] == 'I' && strlen(buf) > 2)
        {
             printf("No such Command\n");
             continue;    
        }
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
