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
char ReceFromFC[MAXSIZE],SendToFC[MAXSIZE],ReceFromBDS[MAXSIZE],SendToBDS[MAXSIZE];
int client_sockfd,sockfd_BDS,sockfd_FC;

struct SB
{
    char valid;
    int cylinder,sector,inodenum,datanum,InodeBitMapLen,DataBitMapLen,InodeBitBlock,DataBitBlock,usedinode,useddata,rootcylinder,rootsector,blockstart;
};

struct Inode
{
    char Time[BLOCKSIZE],Name[BLOCKSIZE];
    int point[12][2];
    int parentcylinder,parentsector,c,s,used,flag,length;
};
struct InodeData
{
    char Name[BLOCKSIZE][BLOCKSIZE];
    int c[BLOCKSIZE],s[BLOCKSIZE],amount;
};
char DataBitMap[100000],InodeBitMap[100000];
SB SuperBlock;
Inode CurrentInode,Root,NewInode;
InodeData CurrentData,RootData,NewData;

int nowcylinder,nowsector;
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

void Show(Inode MyInode,InodeData MyData)
{
    printf("MyInode Time %s\n",MyInode.Time);
    printf("MyInode Name %s\n",MyInode.Name);
    printf("MyInode parentc %d\n",MyInode.parentcylinder);
    printf("MyInode parents %d\n",MyInode.parentsector);
    printf("MyInode c %d\n",MyInode.c);
    printf("MyInode s %d\n",MyInode.s);
    printf("MyInode used %d\n",MyInode.used);
    printf("MyInode flag %d\n",MyInode.flag);
    printf("MyInode length %d\n",MyInode.length);

    printf("MyData %d\n",MyData.amount);
    for (int i = 0;i < MyData.amount;++i)
        printf("%s %d %d\n",MyData.Name[i],MyData.c[i],MyData.s[i]);
}
void Display()
{
    printf("SuperBlock cylinder %d\n",SuperBlock.cylinder);
    printf("SuperBlock sector %d\n",SuperBlock.sector);
    printf("SuperBlock inodenum %d\n",SuperBlock.inodenum);
    printf("SuperBlock datanum %d\n",SuperBlock.datanum);
    printf("SuperBlock InodeBitMapLen %d\n",SuperBlock.InodeBitMapLen);
    printf("SuperBlock DataBitMapLen %d\n",SuperBlock.DataBitMapLen);
    printf("SuperBlock InodeBitBlock %d\n",SuperBlock.InodeBitBlock);
    printf("SuperBlock DataBitBlock %d\n",SuperBlock.DataBitBlock);
    printf("SuperBlock usedinode %d\n",SuperBlock.usedinode);
    printf("SuperBlock useddata %d\n",SuperBlock.useddata);
    printf("SuperBlock rootcylinder %d\n",SuperBlock.rootcylinder);
    printf("SuperBlock rootsector %d\n",SuperBlock.rootsector);
    printf("SuperBlock blockstart %d\n",SuperBlock.blockstart);
    printf("\n");
    for (int i = 0; i < SuperBlock.InodeBitMapLen;++i)
        printf("%c ",InodeBitMap[i]);
    printf("\n");
    for (int i = 0; i < SuperBlock.DataBitMapLen;++i)
        printf("%c ",DataBitMap[i]);
    printf("\n");
    printf("CurrentInode Time %s\n",CurrentInode.Time);
    printf("CurrentInode Name %s\n",CurrentInode.Name);
    printf("CurrentInode parentc %d\n",CurrentInode.parentcylinder);
    printf("CurrentInode parents %d\n",CurrentInode.parentsector);
    printf("CurrentInode c %d\n",CurrentInode.c);
    printf("CurrentInode s %d\n",CurrentInode.s);
    printf("CurrentInode used %d\n",CurrentInode.used);
    printf("CurrentInode flag %d\n",CurrentInode.flag);
    printf("CurrentInode length %d\n",CurrentInode.length);
}
char * itoa(int num,char *str)
{
    char index[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i = 0,j,k;
    unsigned unum;

    unum=(unsigned) num;/*其他情况*/
    /*转换*/
    do{
        str[i++] = index[ unum % (unsigned) 10];
        unum /= 10;
    }while(unum);

    str[i]='\0';
    /*逆序*/
    k=0;
    char temp;
    for(j = k;j <= (i-1)/2; j++)
    {
        temp = str[j];
        str[j] = str[i-1+k-j];
        str[i-1+k-j] = temp;
    }
    return str;
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

void HandleError(const char * const str)
{
    bzero(SendToFC,sizeof(SendToFC));
    strcpy(SendToFC,str);
    Write(client_sockfd,SendToFC,strlen(SendToFC));
    return;
}
bool WriteSuccess()
{
    bzero(ReceFromBDS,sizeof(ReceFromBDS));
    Read(sockfd_BDS,ReceFromBDS,MAXSIZE);
    if (ReceFromBDS[0] == 'Y') return true;
    else return false;
}
void Next(int &cylinder,int &sector)
{
    ++sector;
    if (sector >= SuperBlock.sector)
    {
        sector = 0;
        ++cylinder;
    }
    return;
}
bool GetBlock(InodeData &NewData,int &Acylinder,int &Asector)
{
    int k = -1;
    for (int i = 0;i < SuperBlock.DataBitMapLen;++i) 
        if (DataBitMap[i] == '0')
        {
            k = i;
            break;
        }
    if (k < 0) return 0; 
    DataBitMap[k] = '1';
    k = k + SuperBlock.blockstart;
    Acylinder = k / SuperBlock.sector;
    Asector = k % SuperBlock.sector;
    NewData.amount = 0;
    return 1;
}

void ReadOrder(int cylinder,int sector)
{
    char *tmp = new char[MAXSIZE];

    bzero(SendToBDS,sizeof(SendToBDS));
    strcat(SendToBDS,"R ");
    tmp = itoa(cylinder,tmp);
    strcat(SendToBDS,tmp);
    strcat(SendToBDS," ");
    tmp = itoa(sector,tmp);
    strcat(SendToBDS,tmp);
    strcat(SendToBDS,"\n");
    Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
    
    bzero(ReceFromBDS,sizeof(ReceFromBDS));
    Read(sockfd_BDS,ReceFromBDS,MAXSIZE);
    delete tmp;
}
void ReadInode(Inode & Mynode,int c,int s)
{
    char *str = new char[MAXSIZE],*tmp;
    char *RI = NULL;

    ReadOrder(c,s);
    strcpy(str,ReceFromBDS);
    tmp = str;
    strncpy(Mynode.Time,str,24);
    
    printf("%s\n",tmp);
    str = strtok_r(&str[25]," \n",&RI);
    strcpy(Mynode.Name,str);
    str = strtok_r(NULL," \n",&RI);
    Mynode.parentcylinder = atoi(str);
    str = strtok_r(NULL," \n",&RI);
    Mynode.parentsector = atoi(str);
    str = strtok_r(NULL," \n",&RI);
    Mynode.c = atoi(str);
    str = strtok_r(NULL," \n",&RI);
    Mynode.s = atoi(str);
    str = strtok_r(NULL," \n",&RI);
    Mynode.used = atoi(str);
    str = strtok_r(NULL," \n",&RI);
    Mynode.flag = atoi(str);
    str = strtok_r(NULL," \n",&RI);
    Mynode.length = atoi(str);
    for (int i = 0;i < Mynode.used;++i)
    {     
        str = strtok_r(NULL," \n",&RI);
        Mynode.point[i][0] = atoi(str);
        str = strtok_r(NULL," \n",&RI);
        Mynode.point[i][1] = atoi(str);
    }
    delete tmp;
}
void ReadInodeData(InodeData& MyData,Inode & Mynode)
{
    char *str = new char[MAXSIZE],*tmp;
    char *RID = NULL;

    ReadOrder(Mynode.point[0][0],Mynode.point[0][1]);

    strcpy(str,ReceFromBDS);
    tmp = str;
    str = strtok_r(str," \n",&RID);
    MyData.amount = atoi(str);

    for (int i = 0;i < MyData.amount;++i)
    {
        str = strtok_r(NULL," \n",&RID);
        strcpy(MyData.Name[i],str);

        str = strtok_r(NULL," \n",&RID);
        MyData.c[i] = atoi(str);
        str = strtok_r(NULL," \n",&RID);
        MyData.s[i] = atoi(str);
    }

    delete tmp;
}

bool GetInode(Inode & NewInode, char const *filename,int Choice)
{
    int k = -1;
    printf("337 %s\n",filename);
    for (int i = 0;i < SuperBlock.InodeBitMapLen;++i) 
        if (InodeBitMap[i] == '0')
        {
            k = i;
            break;
        }
    if (k < 0) 
    {
        HandleError("Inode is not enough\n");
        return 0;
    }

    InodeBitMap[k] = '1';
    time_t t;
    t = time(&t);
    strncpy(NewInode.Time,ctime(&t),24);

    strcpy(NewInode.Name,filename);
    NewInode.c = k / SuperBlock.sector;
    NewInode.s = k % SuperBlock.sector;
    NewInode.parentcylinder = CurrentInode.c;
    NewInode.parentsector = CurrentInode.s;
    NewInode.used = 0;
    NewInode.flag = Choice;
    NewInode.length = 0;
    return 1;
}
void initial()
{
    char *str = new char[MAXSIZE],*p;
    int tmp,local,Acylinder,Asector,k;
    nowcylinder = 0;
    nowsector = 0;
    bzero(SendToBDS,sizeof(SendToBDS));
    strcat(SendToBDS,"R 0 0\n");
    Write(sockfd_BDS,SendToBDS,strlen(SendToBDS)); 
    bzero(ReceFromBDS,sizeof(ReceFromBDS));

    Read(sockfd_BDS,ReceFromBDS,MAXSIZE);
    Write(STDOUT_FILENO,ReceFromBDS,strlen(ReceFromBDS));

    bzero(InodeBitMap,sizeof(InodeBitMap));
    bzero(DataBitMap,sizeof(DataBitMap));
    if (ReceFromBDS[0] != '1')
    {
        bzero(SendToBDS,sizeof(SendToBDS));
        strcat(SendToBDS,"I\n");
        Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
        bzero(ReceFromBDS,sizeof(ReceFromBDS));
        Read(sockfd_BDS,ReceFromBDS,MAXSIZE);
        
        strcpy(str,ReceFromBDS);
        p = str;
        str = strtok(str," \n");
        SuperBlock.cylinder = atoi(str);
        str = strtok(str," \n");
        SuperBlock.sector = atoi(str);
        SuperBlock.inodenum = (SuperBlock.cylinder * SuperBlock.sector) / 2;
        SuperBlock.datanum = SuperBlock.cylinder * SuperBlock.sector - SuperBlock.inodenum;

        SuperBlock.blockstart = SuperBlock.inodenum;
        SuperBlock.InodeBitMapLen = SuperBlock.inodenum;

        for (int i = 0;i < SuperBlock.InodeBitMapLen;++i)
            InodeBitMap[i] = '0';

        SuperBlock.DataBitMapLen = SuperBlock.datanum;
        for (int i = 0;i < SuperBlock.DataBitMapLen;++i)
            DataBitMap[i] = '0';

        SuperBlock.InodeBitBlock = SuperBlock.InodeBitMapLen / BLOCKSIZE;
        if (SuperBlock.InodeBitMapLen % BLOCKSIZE > 0) ++SuperBlock.InodeBitBlock;

        SuperBlock.DataBitBlock = SuperBlock.DataBitMapLen / BLOCKSIZE;
        if (SuperBlock.DataBitMapLen % BLOCKSIZE > 0) ++SuperBlock.DataBitBlock;

        SuperBlock.valid = '1';


        /*将BitMap 和superBlock的inode块标记为已用*/
        SuperBlock.usedinode = 1 + SuperBlock.InodeBitBlock + SuperBlock.DataBitBlock;
        SuperBlock.useddata = 0;
        for (int i = 0;i < SuperBlock.usedinode;++i)
            InodeBitMap[i] = '1';

        for (int i = 0;i < SuperBlock.usedinode;++i)
            Next(nowcylinder,nowsector);

        /*建立根目录*/
        if (!GetInode(Root,"/",1)) return;
        SuperBlock.rootcylinder = Root.c;
        SuperBlock.rootsector = Root.s;
        Acylinder = 0;
        Asector = 0;
        GetBlock(RootData,Acylinder,Asector);
        Root.used = 1;
        Root.point[0][0] = Acylinder;
        Root.point[0][1] = Asector;
        Root.parentcylinder = 0;
        Root.parentsector = 0;
        CurrentInode = Root;
        CurrentData = RootData;
    }
    else
    {
        strcpy(str,ReceFromBDS);
        p = str;
        str = strtok(str," \n");
        SuperBlock.valid = str[0];
        str = strtok(NULL," \n");
        SuperBlock.cylinder = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.sector = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.inodenum = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.datanum = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.InodeBitMapLen = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.DataBitMapLen = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.InodeBitBlock = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.DataBitBlock = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.usedinode = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.useddata = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.rootcylinder = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.rootsector = atoi(str);
        str = strtok(NULL," \n");
        SuperBlock.blockstart = atoi(str);

        k = 0;
        for (int i = 0;i < SuperBlock.InodeBitBlock;++i)
        {
            Next(nowcylinder,nowsector);
            ReadOrder(nowcylinder,nowsector);
            for (int j = 0;j < BLOCKSIZE;++j)
            {
                if (k >= SuperBlock.InodeBitMapLen) break;
                InodeBitMap[k] = ReceFromBDS[j];
                ++k;
            }
        }

        k = 0;
        for (int i = 0;i < SuperBlock.DataBitBlock;++i)
        {
            Next(nowcylinder,nowsector);
            ReadOrder(nowcylinder,nowsector);
            for (int j = 0;j < BLOCKSIZE;++j)
            {
                if (k >= SuperBlock.DataBitMapLen) break;
                DataBitMap[k] = ReceFromBDS[j];
                ++k;
            }
        }
        ReadInode(Root,SuperBlock.rootcylinder,SuperBlock.rootsector);  
        ReadInodeData(RootData,Root);
    }
    CurrentInode = Root;
    CurrentData = RootData;
    delete p;

    return;
}

void Format()
{
    bzero(SendToBDS,sizeof(SendToBDS));
    strcpy(SendToBDS,"W 0 0 1 0\n");
    Write(sockfd_BDS,SendToBDS,BLOCKSIZE);
    if (!WriteSuccess())
    {
        HandleError("1:Format fails\n");
        return ;
    }
    initial();
    bzero(SendToFC,sizeof(SendToFC));
    strcat(SendToFC,"Format Successfully\n");
    Write(client_sockfd,SendToFC,strlen(SendToFC));
}

bool EnterFile(Inode &Mynode,InodeData & MyData,char*path)
{
    int k = -1,c,s;
    for (int i = 0;i < MyData.amount;++i)
        if (0 == strcmp(path,MyData.Name[i]))
        {
            k = i;
            break;
        }
    if (k < 0) 
    {
        HandleError("No such file exists\n");
        return false;
    }
    c = MyData.c[k];
    s = MyData.s[k];

    ReadInode(Mynode,c,s);
    if (0 == Mynode.flag)
    {
        HandleError("No such file exists\n");
        return 0;
    }
    ReadInodeData(MyData,Mynode);
    
    return true;    
}
void WriteBack(Inode & MyInode,InodeData & MyData)
{
    char *info = new char[MAXSIZE];
    char *tmp = new char[MAXSIZE];

    bzero(SendToBDS,sizeof(SendToBDS));
    strcat(SendToBDS,"W ");
    tmp = itoa(MyInode.c,tmp);
    strcat(SendToBDS,tmp);
    strcat(SendToBDS," ");
    tmp = itoa(MyInode.s,tmp);
    strcat(SendToBDS,tmp);
    strcat(SendToBDS," ");
    bzero(info,sizeof(info));
    for (int i = 0;i < strlen(MyInode.Time);++i)
        if ('\n' == MyInode.Time[i])
        {
            MyInode.Time[i] = 0;
            break;
        }
    int k ;
    while (strlen(MyInode.Time) < 24)
    {
        k = strlen(MyInode.Time);
        MyInode.Time[k] = 'x';
        MyInode.Time[k + 1] = 0;
    }
    strcat(info,MyInode.Time);
    strcat(info," ");
    strcat(info,MyInode.Name);
    strcat(info," ");

    tmp = itoa(MyInode.parentcylinder,tmp);
    strcat(info,tmp);
    strcat(info," ");

    tmp = itoa(MyInode.parentsector,tmp);
    strcat(info,tmp);
    strcat(info," ");

    tmp = itoa(MyInode.c,tmp);
    strcat(info,tmp);
    strcat(info," ");

    tmp = itoa(MyInode.s,tmp);
    strcat(info,tmp);
    strcat(info," ");

    tmp = itoa(MyInode.used,tmp);
    strcat(info,tmp);
    strcat(info," ");

    tmp = itoa(MyInode.flag,tmp);
    strcat(info,tmp);
    strcat(info," ");

    tmp = itoa(MyInode.length,tmp);
    strcat(info,tmp);
    strcat(info," ");
    for (int i = 0;i < MyInode.used;++i)
    {
        tmp = itoa(MyInode.point[i][0],tmp);
        strcat(info,tmp);
        strcat(info," ");
        tmp = itoa(MyInode.point[i][1],tmp);
        strcat(info,tmp);
        strcat(info," ");
    }
    strcat(SendToBDS,itoa(strlen(info),tmp));
    strcat(SendToBDS," ");
    strcat(SendToBDS,info);
    strcat(SendToBDS,"\n");
    Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
    if (!WriteSuccess())
    {
        HandleError("Write error\n");
        return;
    };
    if (MyInode.flag == 1)
    {
        bzero(SendToBDS,sizeof(SendToBDS));
        bzero(info,sizeof(info));
        tmp = itoa(MyInode.point[0][0],tmp);
        strcat(SendToBDS,"W ");
        strcat(SendToBDS,tmp);
        strcat(SendToBDS," ");
        tmp = itoa(MyInode.point[0][1],tmp);
        strcat(SendToBDS,tmp);
        strcat(SendToBDS," ");
        tmp = itoa(MyData.amount,tmp);
        strcat(info,tmp);
        strcat(info," ");
        for (int i = 0;i < MyData.amount;++i)
        {
            strcat(info,MyData.Name[i]);
            strcat(info," ");
            tmp = itoa(MyData.c[i],tmp);
            strcat(info,tmp);
            strcat(info," ");
            tmp = itoa(MyData.s[i],tmp);
            strcat(info,tmp);
            strcat(info," ");
        }
        strcat(SendToBDS,itoa(strlen(info),tmp));
        strcat(SendToBDS," ");
        strcat(SendToBDS,info);
        strcat(SendToBDS,"\n");
        Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
        if (!WriteSuccess())
        {
            HandleError("Write error\n");
            return;
        };
    }
    delete info;
    delete tmp;
    return;
}
int Changedir(char *path)
{
    bool Check;
    Inode TmpInode;
    InodeData TmpData;
    char *CD = NULL;

    if (strlen(path) == 1 && path[0] == '.') return 1;
    else
    {
        if (path[0] == '/')
        {
            ReadInode(TmpInode,SuperBlock.rootcylinder,SuperBlock.rootsector);
            ReadInodeData(TmpData,TmpInode);
            path = strtok_r(&path[1],"/",&CD);
            while (path)
            {
                Check = EnterFile(TmpInode,TmpData,path);
                if (!Check) 
                {
                    HandleError("1:No such file or directory\n");
                    return 0;
                }
                path = strtok_r(NULL,"/",&CD);
            }
        }
        else
        {
            TmpInode = CurrentInode;
            TmpData = CurrentData;
            if (0 == strcmp(path,".."))
            {
                if (TmpInode.c == SuperBlock.rootcylinder && TmpInode.s == SuperBlock.rootsector)
                {
                    HandleError("1:No such file exists\n");
                    return 0;
                }
                ReadInode(TmpInode,TmpInode.parentcylinder,TmpInode.parentsector);
                ReadInodeData(TmpData,TmpInode);
            }
            else
            {
                path = strtok_r(path,"/",&CD);
                while (path)
                {
                    Check = EnterFile(TmpInode,TmpData,path);
                    if (!Check)
                    {
                        return 0;
                    }
                    path = strtok_r(NULL,"/",&CD);
                }
            }
        }
    }

    WriteBack(CurrentInode,CurrentData);
    CurrentInode = TmpInode;
    CurrentData = TmpData;
    Show(CurrentInode,CurrentData);
    return 1;
}


int Create(char *filename,int Choice)
{
    printf("%s\n",filename);
    for (int i = 0; i < CurrentData.amount;++i)
        if (!strcmp(CurrentData.Name[i],filename))
        {
            HandleError("1:The file has existed\n");
            return 0;
        }

    if (!GetInode(NewInode,filename,Choice))  return 0;
    strncpy(CurrentData.Name[CurrentData.amount],filename,strlen(filename));
    CurrentData.Name[CurrentData.amount][strlen(filename)] = 0;
    printf("746 %s\n",CurrentData.Name[CurrentData.amount]);
    CurrentData.c[CurrentData.amount] = NewInode.c;
    CurrentData.s[CurrentData.amount] = NewInode.s;
    ++CurrentData.amount;

    if (1 == Choice)
    {
        int Acylinder,Asector;
        if (!GetBlock(NewData,Acylinder,Asector))  return 0;
        NewInode.used = 1;
        NewInode.point[0][0] = Acylinder;
        NewInode.point[0][1] = Asector;
    }
    WriteBack(CurrentInode,CurrentData);
    WriteBack(NewInode,NewData);
    return 1;
}

void Createfile(char *path,int Choice)
{
    int k = -1;
    char filename[BLOCKSIZE];
    Inode TempInode;
    InodeData TempData;

    for (int i = strlen(path);i >= 0;--i)
        if ('/' == path[i]) 
        {
            k = i;
            path[k] = 0;
            break;
        }
  if (k > -1) strcpy(filename,&path[k + 1]);
  else strcpy(filename,path);
  filename[strlen(path)] = 0;

    TempInode = CurrentInode;
    TempData = CurrentData;
    if (k > -1 && (filename == NULL || !Changedir(path)))
    {
        HandleError("2:No such directory\n");
        return;
    };

    if (!Create(filename,Choice))  
    {
        if (k > -1)
        {
            CurrentInode = TempInode;
            CurrentData = TempData;
        }
        return;
    }
    bzero(SendToFC,sizeof(SendToFC));
    strcpy(SendToFC,"0:successfully create the file\n");
    Write(client_sockfd,SendToFC,strlen(SendToFC));
    return ;
}


int Remove(char *filename,int Choice)
{
    int k = -1;
    for (int i = 0; i < CurrentData.amount;++i)
        if (!strcmp(CurrentData.Name[i],filename))
        {
            k = i;
            break;
        }

    if (-1 == k) 
    {   printf("No such file\n");
        return 0;
    }
    --CurrentData.amount;
    strncpy(CurrentData.Name[k],CurrentData.Name[CurrentData.amount],strlen(CurrentData.Name[CurrentData.amount]));
    CurrentData.c[k] = CurrentData.c[CurrentData.amount];
    CurrentData.s[k] = CurrentData.s[CurrentData.amount];
    WriteBack(CurrentInode,CurrentData);

    return 1;
}
void Removefile(char *path,int Choice)
{
    int k = -1;
    char filename[BLOCKSIZE];
    Inode TempInode;
    InodeData TempData;

    for (int i = strlen(path);i >= 0;--i)
        if ('/' == path[i]) 
        {
            k = i;
            path[k] = 0;
            break;
        }

    strcpy(filename,&path[k + 1]);
    TempInode = CurrentInode;
    TempData = CurrentData;
    if (k > -1 && (filename == NULL || !Changedir(path)))
    {
        HandleError("1:No such file or directory\n");
        return;
    };
    if (!Remove(filename,Choice))  
    {
        HandleError("1:No such file or directory\n");
        if (k > -1)
        {
            CurrentInode = TempInode;
            CurrentData = TempData;
        }
        return;
    }
    bzero(SendToFC,sizeof(SendToFC));
    strcpy(SendToFC,"0:successfully remove the file\n");
    Write(client_sockfd,SendToFC,strlen(SendToFC));
    return ;
}
void List(char *argument)
{
    char *str = new char [MAXSIZE];
    Inode TmpInode;
    
    if (0 == CurrentData.amount)
    {
        bzero(SendToFC,sizeof(SendToFC));
        strcat(SendToFC,"\n");
        Write(client_sockfd,SendToFC,strlen(SendToFC));
    }
    else
    {
        if (argument[0] == '0')
        {
            bzero(SendToFC,sizeof(SendToFC));
            for (int i = 0;i < CurrentData.amount;++i)
            {
                strcat(SendToFC,CurrentData.Name[i]);
                strcat(SendToFC,"\n");
            }
            Write(client_sockfd,SendToFC,strlen(SendToFC));
        }
        else
        {
            bzero(SendToFC,sizeof(SendToFC));
            for (int i = 0;i < CurrentData.amount;++i)
            {
                strcat(SendToFC,CurrentData.Name[i]);
                strcat(SendToFC," created time: ");
                ReadInode(TmpInode,CurrentData.c[i],CurrentData.s[i]);
                strcat(SendToFC,TmpInode.Time);
                strcat(SendToFC," ");
                str = itoa(TmpInode.length,str);
                strcat(SendToFC,str);
                strcat(SendToFC,"\n");    
            }
            Write(client_sockfd,SendToFC,strlen(SendToFC));
        }
    }
    delete str;
    return ;
}

char * ReadData(char *str,Inode Node)
{
    int c,s;
    bzero(str,sizeof(str));
    strcpy(str,"");
    for (int i = 0;i < Node.used;++i)
    { 
        c = Node.point[i][0];
        s = Node.point[i][1];
        ReadOrder(c,s);
        strcat(str,ReceFromBDS);
    }
    return str;
}
int Catch(char *filename)
{
    int k = -1;
    Inode TmpInode;
    char *str = new char[MAXSIZE];


    for (int i = 0;i < CurrentData.amount;++i)
        if (!strcmp(filename,CurrentData.Name[i]))
        {
            k = i;
            break;
        }
    if (k < 0) 
    { 
        delete str;
        HandleError("1:No such filename exists\n");
        return 0;
    }
    ReadInode(TmpInode,CurrentData.c[k],CurrentData.s[k]);
    if (1 == TmpInode.flag)
    {
        delete str;
        HandleError("1:No such filename exists\n");
        return 0;
    }
    bzero(SendToFC,sizeof(SendToFC));
    strcat(SendToFC,"0 ");
    str = itoa(TmpInode.length,str);
    strcat(SendToFC,str);
    strcat(SendToFC," ");
    str = ReadData(str,TmpInode);
    strcat(SendToFC,str);
    strcat(SendToFC,"\n");
    Write(client_sockfd,SendToFC,strlen(SendToFC));
    
    delete str;
    return 1;
}
void Catchfile(char *path)
{
    int k = -1;
    char filename[BLOCKSIZE];
    Inode TempInode;
    InodeData TempData;

    for (int i = strlen(path);i >= 0;--i)
        if ('/' == path[i]) 
        {
            k = i;
            path[k] = 0;
            break;
        }
    strcpy(filename,&path[k + 1]);
    TempInode = CurrentInode;
    TempData = CurrentData;
    if (k > -1 && (filename == NULL || !Changedir(path)))
    {
        HandleError("1:No such file or directory\n");
        return;
    };
    if (!Catch(filename))  
    {
        if (k > -1)
        {
            CurrentInode = TempInode;
            CurrentData = TempData;
        }
        return;
    }
    if (k > -1)
    {
        CurrentInode = TempInode;
        CurrentData = TempData;
    }
    return;
}

void WriteData(char *path)
{
    char *filename,*tmp,*data;
    char *WD = NULL;
    int len,k,Acylinder,Asector;
    Inode TmpInode;
    InodeData TmpData;

    filename = strtok_r(path," \n",&WD);
    tmp = strtok_r(NULL," \n",&WD);
    len = atoi(tmp);
    data = strtok_r(NULL," \n",&WD);
    k = -1;
    for (int i = 0;i < CurrentData.amount;++i)
        if (0 == strcmp(filename,CurrentData.Name[i]))
        {
            k = i;
            break;
        }
    if (k < 0) 
    {
        HandleError("1:No such file exists\n");
        return;
    }

    ReadInode(TmpInode,CurrentData.c[k],CurrentData.s[k]);
    if (1 == TmpInode.flag)
    {
        HandleError("1:No such file exist\n");
        return;
    }
    else
    {
        TmpInode.length = len;
        TmpInode.used = len / BLOCKSIZE;
        if (0 != len % BLOCKSIZE) ++TmpInode.used;
        for (int i = 0;i < TmpInode.used;++i)
        {
            if (TmpInode.point[i][0] == 0 && TmpInode.point[i][1] == 0)
            {
                GetBlock(TmpData,Acylinder,Asector);
                TmpInode.point[i][0] = Acylinder;
                TmpInode.point[i][1] = Asector; 
            }
            else
            {
                Acylinder = TmpInode.point[i][0];
                Asector = TmpInode.point[i][1];
            }
            bzero(SendToBDS,sizeof(SendToBDS));
            strcat(SendToBDS,"W ");
            tmp = itoa(Acylinder,tmp);
            strcat(SendToBDS,tmp);
            strcat(SendToBDS," ");
            tmp = itoa(Asector,tmp);
            strcat(SendToBDS,tmp);
            strcat(SendToBDS," ");
            if (i < TmpInode.used - 1) tmp = itoa(BLOCKSIZE,tmp);
            else tmp = itoa(len % BLOCKSIZE,tmp);
            strcat(SendToBDS,tmp);
            strcat(SendToBDS," ");
            data = &data[i * BLOCKSIZE];
            tmp = strncpy(tmp,data,BLOCKSIZE);
            strcat(SendToBDS,tmp);
            strcat(SendToBDS,"\n");
            Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
            if (!WriteSuccess())
            {
                 HandleError("2:Wrong Write\n");
                 return;
            }
        }
        WriteBack(TmpInode,TmpData);
    }
    HandleError("0:successfully written file\n");
    return ;
}


void Append(char *path)
{
    char *filename;
    char *tmp;
    char *data;
    char *tmpdata = new char[MAXSIZE];
    char *writetmp = new char[MAXSIZE];
    int len,k,tmplen;
    char *Ad = NULL;
    Inode TmpInode;

    filename = strtok_r(path," \n",&Ad);
    tmp = strtok_r(NULL," \n",&Ad);
    len = atoi(tmp);
    data = strtok_r(NULL,"\n",&Ad);
    k = -1;
    for (int i = 0;i < CurrentData.amount;++i)
        if (0 == strcmp(filename,CurrentData.Name[i]))
        {
            k = i;
            break;
        }
    if (k < 0) 
    {
        HandleError("1:No such file exists\n");
        return;
    }
    ReadInode(TmpInode,CurrentData.c[k],CurrentData.s[k]);
    if (1 == TmpInode.flag)
    {
        HandleError("1:No such file exist\n");
        return;
    }
    else
    {
        tmplen = TmpInode.length + len;
        bzero(tmpdata,sizeof(tmpdata));
        if (TmpInode.length > 0) 
         { 
            tmpdata = ReadData(tmpdata,TmpInode);
            tmpdata[strlen(tmpdata) - 1] = 0;
         }
        strcat(tmpdata,data);
        bzero(writetmp,sizeof(writetmp));
        strcpy(writetmp,filename);
        strcat(writetmp," ");
        strcat(writetmp,itoa(tmplen,tmp));
        strcat(writetmp," ");
        strcat(writetmp,tmpdata);
        strcat(writetmp,"\n");
        WriteData(writetmp);
    }
    delete writetmp;
    delete tmpdata;
    return;
}

void WriteOrder(int c,int s,char *str)
{
    char *tmp = new char[MAXSIZE];

    bzero(SendToBDS,sizeof(SendToBDS));
    strcat(SendToBDS,"W ");

    tmp = itoa(c,tmp);
    strcat(SendToBDS,tmp);
    strcat(SendToBDS," ");

    tmp = itoa(s,tmp);
    strcat(SendToBDS,tmp);
    strcat(SendToBDS," ");

    tmp = itoa(strlen(str),tmp);
    strcat(SendToBDS,tmp);
    strcat(SendToBDS," ");
    strcat(SendToBDS,str);
    strcat(SendToBDS,"\n");

    Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));

    if (!WriteSuccess())
    {
        HandleError("Write error\n");
        return;
    };
}
void Exit()
{
    char *info = new char[MAXSIZE];
    char *tmp = new char [MAXSIZE];
    char *str = new char[MAXSIZE];
    int len;

    /*SuperBlock*/
    bzero(info,sizeof(info));
    info[0] = SuperBlock.valid;
    info[1] = 0;
    strcat(info," ");

    tmp = itoa(SuperBlock.cylinder,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.sector,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.inodenum,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.datanum,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.InodeBitMapLen,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.DataBitMapLen,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.InodeBitBlock,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.DataBitBlock,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.usedinode,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.useddata,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.rootcylinder,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.rootsector,tmp);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.blockstart,tmp);
    info = strcat(info,tmp);
    info = strcat(info,"\n");

    len = strlen(info);
    tmp = itoa(len,tmp);
    tmp = strcat(tmp," ");

    bzero(SendToBDS,sizeof(SendToBDS));
    strcat(SendToBDS,"W 0 0 ");
    strcat(SendToBDS,tmp);
    strcat(SendToBDS,info);
    Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
    if (!WriteSuccess())
    {
        HandleError("Write error\n");
        return;
    };

    /*InodeBitMap*/
    nowcylinder = 0;
    nowsector = 0;
    int k = 0;
    for (int i = 0;i < SuperBlock.InodeBitBlock;++i)
    {
        Next(nowcylinder,nowsector);
        strncpy(str,&InodeBitMap[i * BLOCKSIZE],BLOCKSIZE);
        WriteOrder(nowcylinder,nowsector,str);
    }
    /*DataBitMap*/
    for (int i = 0;i < SuperBlock.DataBitBlock;++i)
    {
        Next(nowcylinder,nowsector);
        strncpy(str,&InodeBitMap[i * BLOCKSIZE],BLOCKSIZE);
        WriteOrder(nowcylinder,nowsector,str);
    }

    /*CurrentInode*/
    WriteBack(CurrentInode,CurrentData);

    delete info;
    delete tmp;
    delete str;
    return;
}
