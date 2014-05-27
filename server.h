#include <sys/mman.h>
#include <pthread.h>
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
#include <queue>
#include <vector>
const int MAXSIZE = 8192;
const int BLOCKSIZE = 256;
int cylinders,sectors,delay,nowcylinder = 0;
char * diskfile;
char recebuf[MAXSIZE],sendbuf[MAXSIZE];
int client_sockfd;
pthread_t tid;

struct Node
{
    char com;
    int r,s,len;
    char data[MAXSIZE];
};

struct CmpSSTF
{
    bool operator()(Node a,Node b)
    {
        return abs(a.r - nowcylinder) >= abs(b.r - nowcylinder);
    }
};
struct CmpC_LOOK
{
    bool operator()(Node a,Node b)
    {
        int delta_a,delta_b;

        delta_a = a.r - nowcylinder;
        delta_b = b.r - nowcylinder;
        if (delta_a >= 0 && delta_b < 0) return false;
        if (delta_a < 0 && delta_b >= 0) return true;
        return delta_a >= delta_b;
    }
};
std::queue <Node> CacheFCFS;
std::priority_queue <Node,std::vector<Node>,CmpSSTF> CacheSSTF;
std::priority_queue <Node,std::vector<Node>,CmpC_LOOK> CacheC_LOOK;

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
    diskfile = (char *)mmap(0,filesize,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0644);

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
    strcat(sendbuf,str);
    strcat(sendbuf," ");

    str = itoa(sectors,str);
    strcat(sendbuf,str);
    strcat(sendbuf,"\n");
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
void init(char *p,Node & Mynode)
{
    char *in_ptr = NULL;

    p = strtok_r(p," ",&in_ptr);
    Mynode.com = p[0];
    if (Mynode.com == 'W' || Mynode.com == 'R')
    {
        p = strtok_r(NULL," ",&in_ptr);
        Mynode.r = atoi(p);
        p = strtok_r(NULL," ",&in_ptr);
        Mynode.s = atoi(p);
        if (Mynode.com == 'W')
        {
            p = strtok_r(NULL," ",&in_ptr);
            Mynode.len = atoi(p);
            p = strtok_r(NULL," ",&in_ptr);
            strcpy(Mynode.data,p);
        }
    }
}

void handle(Node Mynode)
{
    char str[MAXSIZE];
    int len,time;

    bzero(sendbuf,sizeof(sendbuf));
    if (Mynode.com == 'I')
    {
        itoa(cylinders,str);
        strcat(sendbuf,str);
        strcat(sendbuf," ");

        itoa(sectors,str);
        strcat(sendbuf,str);
        strcat(sendbuf,"\n");
    }
    else
    {
        if (Mynode.r >= cylinders || Mynode.s >= sectors)
        {
            strcat(sendbuf,"No\n");
            return;
        }
        else
        {
            time = abs(Mynode.r - nowcylinder) * delay;
            usleep(time);
            nowcylinder = Mynode.r;
            switch (Mynode.com)
            {
                case 'R':strcat(sendbuf,"Yes ");
                         memcpy(&sendbuf[4],&diskfile[BLOCKSIZE * (Mynode.r * sectors + Mynode.s)],BLOCKSIZE);
                         strcat(sendbuf,"\n");
                         break;
                case 'W':strcat(sendbuf,"Yes\n");
                         memcpy(&diskfile[BLOCKSIZE * (Mynode.r * sectors + Mynode.s)],Mynode.data,len);
                         break;
            }
        }
    }
}
//pthread
void *C_LOOK(void *arg)
{
    Node Mynode;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
    while (1)
    {
        pthread_testcancel();
        usleep(1);
        pthread_testcancel();
        if (!CacheC_LOOK.empty())
        {
            Mynode = CacheC_LOOK.top();
            CacheC_LOOK.pop();
            handle(Mynode);
            Write(client_sockfd,sendbuf,strlen(sendbuf));
        }
    }
}
void *SSTF(void *arg)
{
    Node Mynode;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
    while (1)
    {
        pthread_testcancel();
        usleep(1);
        pthread_testcancel();
        if (!CacheSSTF.empty())
        {
            Mynode = CacheSSTF.top();
            CacheSSTF.pop();
            handle(Mynode);
            Write(client_sockfd,sendbuf,strlen(sendbuf));
        }
    }
}
void *FCFS(void *arg)
{
    Node Mynode;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
    while (1)
    {
        pthread_testcancel();
        usleep(1);
        pthread_testcancel();
        if (!CacheFCFS.empty())
        {
            Mynode = CacheFCFS.front();
            CacheFCFS.pop();
            printf("%c %d %d successfully\n",Mynode.com,Mynode.r,Mynode.s);
            handle(Mynode);
            Write(client_sockfd,sendbuf,strlen(sendbuf));
        }
    }
}

