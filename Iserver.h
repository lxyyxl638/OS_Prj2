#include <sys/mman.h>
#include <iostream>
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
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
bool flag;
int num = 0;
using namespace std;
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
        if (a.com == 'E') return true;
        if (b.com == 'E') return false;
        return abs(a.r - nowcylinder) >= abs(b.r - nowcylinder);
    }
};
struct CmpC_LOOK
{
    bool operator()(Node a,Node b)
    {
        int delta_a,delta_b;

        if (a.com == 'E') return true;
        if (b.com == 'E') return false;
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
Node Mynode;
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




int geti(char *str,int & result)
{
    int i = 0,k = 0;
    char tmpbuf[MAXSIZE];

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

void init(char *p,Node & Mynode)
{
    char *in_ptr = NULL;
    char *tmp = new char[MAXSIZE];
    char *del;

    strcpy(tmp,p);
    del = tmp;
    tmp = strtok_r(tmp," \n",&in_ptr);
    Mynode.com = tmp[0];
    if (Mynode.com == 'W' || Mynode.com == 'R')
    {
        tmp = strtok_r(NULL," ",&in_ptr);
        Mynode.r = atoi(tmp);
        tmp = strtok_r(NULL," ",&in_ptr);
        Mynode.s = atoi(tmp);
        if (Mynode.com == 'W')
        {
            tmp = strtok_r(NULL," ",&in_ptr);
            Mynode.len = atoi(p);
            tmp = strtok_r(NULL," ",&in_ptr);
            strcpy(Mynode.data,tmp);
        }
    }
    delete del;
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
                case 'W': 
                         strcat(sendbuf,"Yes\n");
                         memcpy(&diskfile[BLOCKSIZE * (Mynode.r * sectors + Mynode.s)],Mynode.data,strlen(Mynode.data));
                         break;
            }
        }
    }
}
//pthread
void *C_LOOK(void *arg)
{
    Node Mynode;
    while (1)
    {
        if (!CacheC_LOOK.empty())
        {      
            pthread_mutex_lock(&mutex);
            Mynode = CacheC_LOOK.top();
            CacheC_LOOK.pop();
            pthread_mutex_unlock(&mutex); 
            if (Mynode.com == 'E') 
            {
                flag = true;
                bzero(sendbuf,sizeof(sendbuf));
                strcat(sendbuf,"EXIT\n");
                Write(client_sockfd,sendbuf,strlen(sendbuf));
                cout << flush;
                return (void *) 1;
            }
            handle(Mynode);
            Write(client_sockfd,sendbuf,strlen(sendbuf));
        }
    }
}
void *SSTF(void *arg)
{
    while (1)
    {  
        if (!CacheSSTF.empty())
        {
            pthread_mutex_lock(&mutex);
            Mynode = CacheSSTF.top();
            CacheSSTF.pop();
            pthread_mutex_unlock(&mutex);
            if (Mynode.com == 'E') 
            {
                flag = true;
                bzero(sendbuf,sizeof(sendbuf));
                strcat(sendbuf,"EXIT\n");
                Write(client_sockfd,sendbuf,strlen(sendbuf));
                return (void *) 1;
            }
            handle(Mynode);
            Write(client_sockfd,sendbuf,strlen(sendbuf));
        }
    }
}
void *FCFS(void *arg)
{
    Node Mynode;
    while (1)
    {
        if (!CacheFCFS.empty())
        {

            pthread_mutex_lock(&mutex);
            Mynode = CacheFCFS.front();
            CacheFCFS.pop();
            pthread_mutex_unlock(&mutex);
            if (Mynode.com == 'E') 
            {
                flag = true;
                bzero(sendbuf,sizeof(sendbuf));
                strcat(sendbuf,"EXIT\n");
                Write(client_sockfd,sendbuf,strlen(sendbuf));
                return (void *) 1;
            }
            handle(Mynode);
            Write(client_sockfd,sendbuf,strlen(sendbuf));
        }
    }
}


