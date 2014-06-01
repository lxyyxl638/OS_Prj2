/*For my SuperY*/
/*Never Give Up*/
#include "Bserver.h"

int main(int argc,char* argv[])
{
    char buf[MAXSIZE],sendbuf[MAXSIZE];
    char *filename = argv[1];
    int cylinders = atoi(argv[2]);
    int sectors = atoi(argv[3]);
    int delay = atoi(argv[4]);
    int port = atoi(argv[5]);

    int fd = Open(filename);
    
    long filesize = BLOCKSIZE * sectors * cylinders;
    int result = Lseek(fd,filesize - 1);

    Mmap(diskfile,filesize,fd);
    
    int sockfd = Socket();
    struct sockaddr_in serv_addr;

    struct sockaddr_in client_addr;
    int nread;
    socklen_t len;
    int nowcylinder = 0;

    Bind(sockfd,serv_addr,port);
    Listen(sockfd,5);
    printf("The server is listening\n");
    while (1)
    {
       len = sizeof(client_addr);
       client_sockfd = accept(sockfd,(struct sockaddr *) & client_addr, &len);
     printf("Connect successfully\n");
       while (1)
       {
             bzero(buf,sizeof(buf));
             nread = Read(client_sockfd,buf,sizeof(buf));
             if (0 == nread)
             {
                 printf("The other size has been closed.\n");
                 break;
             }
          
          /*   switch(buf[0])
            {
              case 'I':queryI(cylinders,sectors,sendbuf);break;
              case 'R':queryR(diskfile,cylinders,sectors,buf,nowcylinder,delay,sendbuf);break;
              case 'W':queryW(diskfile,cylinders,sectors,buf,nowcylinder,delay,sendbuf);break;
            }
           */
            Write(client_sockfd,buf,strlen(buf)); 
           // Write(client_sockfd,sendbuf,strlen(sendbuf));
             
       }
       Close(client_sockfd);
    }
    Close(sockfd);
}

