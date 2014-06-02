/*For my SuperY*/
/*Never Give Up*/
#include "Bserver.h"
int main(int argc,char* argv[])
{
    char *filename = argv[1];
    int port = atoi(argv[5]);

    int fd = Open(filename);
    
    char *out_ptr = NULL,*p = new char[MAXSIZE];
    char str[MAXSIZE];
    long filesize;
    int result,sockfd;
        
    struct sockaddr_in serv_addr,client_addr;
    int nread;
    socklen_t len;
    
    cylinders = atoi(argv[2]);
    sectors = atoi(argv[3]);
    delay = atoi(argv[4]);
    filesize = BLOCKSIZE * sectors * cylinders;
    result = Lseek(fd,filesize - 1);
    Mmap(diskfile,filesize,fd);

    sockfd = Socket();
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
             bzero(recebuf,sizeof(recebuf));
             nread = Read(client_sockfd,recebuf,MAXSIZE);
             Write(STDOUT_FILENO,recebuf,strlen(recebuf));
             if (0 == nread)
             {
                 printf("The other size has been closed.\n");
                 break;
             }
            strcpy(str,recebuf);
            if (p != NULL)
            {
                strcat(p,str);
                strcpy(str,p);
            }
           
            p = strtok_r(str,"\n",&out_ptr);

            while (p)
            {
                init(p,Mynode);
                handle(Mynode);
                p = strtok_r(NULL,"\n",&out_ptr);
            } 
       }
       Close(client_sockfd);
    }
    Close(sockfd);
}

