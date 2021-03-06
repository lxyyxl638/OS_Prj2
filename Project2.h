#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cctype>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <ctime>

const int MAXSIZE = 1024;
const int BLOCKSIZE = 256;
int Open(char* filename)
{
    int fd = open(filename,O_RDWR|O_CREAT,0);
    if (fd < 0)
    {
        printf("Error:Could not open file '%s'.\n",filename);
        exit(-1);
    }
    return fd;
}

int Lseek(int fd,int size)
{
    int result = lseek(fd,size,SEEK_SET);
    if (result == -1)
    {
        perror("Error calling lseek() to 'stretch the file'");
        close(fd);
        exit(-1);
    }
    result = write(fd,"",1);
    if (result != 1)
    {
        perror("Error writing last byte of the file");
        close(fd);
        exit(-1);
    }
    return result;
}

void Mmap(char* & diskfile,int filesize,int fd)
{
    diskfile = (char *)mmap(0,filesize,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);

    if (diskfile == MAP_FAILED)
    {
        close(fd);
        printf("Error:Could not map file.\n");
        exit(-1);
    }
}

int Socket()
{
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        exit(2);
    }
    return sockfd;
}

void Bind(int sockfd,struct sockaddr_in & serv_addr,int port)
{
    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if (bind(sockfd,(sockaddr*)&serv_addr,sizeof(serv_addr)) < 0)
    {
        perror("ERROR bind");
        exit(2);
    }
}

void Listen(int fd,int backlog)
{
    if (listen(fd,backlog) < 0)
    {
        perror("ERROR listen");
        exit(2);
    }
}

ssize_t Read(int fd,void *ptr,size_t nbytes)
{
    ssize_t n;

again:
    if ((n = read(fd,ptr,nbytes)) == -1)
    {
        if (errno == EINTR)
            goto again;
        else 
            return -1;
    }
    return n;
}

ssize_t Write(int fd,const void *ptr,size_t nbytes)
{
    ssize_t n;
again:
    if ((n = write(fd,ptr,nbytes)) == -1)
    {
        if (errno == EINTR)
            goto again;
        else
            return -1;
    }
    return n;
}

void Close(int fd)
{
   if (close(fd) == -1)
   {
       perror("Error Close");
       exit(-1);
   }
}

char *itoa(int x,char *str)
{
    int div = 1,tmp,k = 0;
    bzero(str,sizeof(str));
    while (div <= x) div *= 10;
    div /= 10;
    while (x > 0 || div > 0)
    {
        tmp = x;
        str[k++] = (tmp / div) + '0';
        x %= div;
        div = div / 10;
    }
   return str; 
}
void queryI(int cylinders,int sectors,char* sendbuf)
{
    int nowcylinder,nowsector;
    char* str = new char[MAXSIZE];
    bzero(sendbuf,sizeof(sendbuf));
    str = itoa(cylinders,str);
    sendbuf = strcat(sendbuf,str);
    sendbuf = strcat(sendbuf," ");
     
    str = itoa(sectors,str);
    sendbuf = strcat(sendbuf,str);
    sendbuf = strcat(sendbuf,"\n");
}

void queryR(char * & diskfile,int cylinders,int sectors,char buf[],int lastcylinder,int delay,char *sendbuf)
{
    char tmpbuf[10];
    int i = 2,k = 0,time,nowcylinder,nowsector;
    
    while (i < strlen(buf))
    {
        if (buf[i] >= '0' && buf[i] <= '9') 
             tmpbuf[k++] = buf[i];
        else break;
        ++i;
    }
    tmpbuf[k] = 0;
    
    nowcylinder = atoi(tmpbuf);
    
    ++i;
    k = 0;
    while (i < strlen(buf))
    {
        if (buf[i] >= '0' && buf[i] <= '9') 
             tmpbuf[k++] = buf[i];
        else break;
        ++i;
    }
    tmpbuf[k] = 0;
    
    nowsector = atoi(tmpbuf);
    
    if (nowcylinder > cylinders || nowsector > sectors) 
    {
        bzero(sendbuf,sizeof(sendbuf));
        strcat(sendbuf,"No\n");
        return;
    }
    else
    {
        bzero(sendbuf,sizeof(sendbuf));
        strcat(sendbuf,"Yes ");
        memcpy( &sendbuf[4], &diskfile[BLOCKSIZE*(nowcylinder * sectors + nowsector)],BLOCKSIZE);
        strcat(sendbuf,"\n"); 
        time = abs(nowcylinder - lastcylinder) * delay;
        usleep(time);
    }
}

int geti(char *str,int & result)
{
    int i = 0,k = 0;
    char * tmpbuf = new char[MAXSIZE];

    while (i < strlen(str))
    {
        if (str[i] >= '0' && str[i] <= '9') 
             tmpbuf[k++] = str[i];
        else break;
        ++i;
    }
    tmpbuf[k] = 0;
    result = atoi(tmpbuf);
    ++i;

    return i;
}
void queryW(char* & diskfile,int cylinders,int sectors,char* buf,int lastcylinder,int delay,char * sendbuf)
{
    char* tmpbuf = new char[10];
    int i = 2,k = 0,time,nowcylinder,nowsector;

    while (i < strlen(buf))
    {
        if (buf[i] >= '0' && buf[i] <= '9') 
             tmpbuf[k++] = buf[i];
        else break;
        ++i;
    }
    tmpbuf[k] = 0;
    nowcylinder = atoi(tmpbuf);
    ++i;
    k = 0;
    while (i < strlen(buf))
    {
        if (buf[i] >= '0' && buf[i] <= '9') 
             tmpbuf[k++] = buf[i];
        else break;
        ++i;
    }
    tmpbuf[k] = 0;
  
  
    nowsector = atoi(tmpbuf);

    ++i;
    k = 0;
    while (i < strlen(buf))
    {
        if (buf[i] >= '0' && buf[i] <= '9') 
             tmpbuf[k++] = buf[i];
        else break;
        ++i;
    }
    tmpbuf[k] = 0;
    
    int len = atoi(tmpbuf);
    ++i;
      
    if (nowcylinder > cylinders || nowsector > sectors || len > BLOCKSIZE) 
    {
        bzero(sendbuf,sizeof(sendbuf));
        strcat(sendbuf,"No\n");
        return;
    }
    else
    {
        bzero(sendbuf,sizeof(sendbuf));
        strcat(sendbuf,"Yes\n");
        memcpy(&diskfile[BLOCKSIZE * (nowcylinder * sectors + nowsector)],&buf[i],len);
        time = abs(nowcylinder - lastcylinder) * delay;
        usleep(time);
    }
    return;
}
