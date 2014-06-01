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

char ReceFromFC[MAXSIZE],SendToFC[MAXSIZE],ReceFromBDS[MAXSIZE],SendToBDS[MAXSIZE];
int client_sockfd,sockfd_BDS,sockfd_FC;


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

void HandleError(char *str)
{
    printf("%s",str);
    return;
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
void GetBlock(InodeData &NewData,int &Acylinder,int &Asector)
{
    int k;
    for (int i = 0;i < SuperBlock.DataBitMapLen;++i) 
        if (DataBitMap[i] == 0)
        {
            k = i;
            break;
        }
    DataBitMap[k] = 1;
    k = k + SuperBlock.blockstart;
    Acylinder = k / BLOCKSIZE;
    Asector = k % BLOCKSIZE;
    NewData.amount = 0;
}

void ReadOrder(int cylinder,int sector)
{
    char tmp[10];
    bzero(SendToBDS,sizeof(SendToBDS));
    strcat(SendToBDS,"R ");
    tmp = itoa(cylinder);
    strcat(SendToBDS,tmp);
    strcat(SendToBDS," ");
    tmp = itoa(sector);
    strcat(SendToBDS,tmp);
    strcat(SendToBDS,"\n");
    Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
    Read(sockfd_BDS,ReceFromBDS,MAXSIZE);
}
void ReadInode(Inode & Mynode,int c,int s)
{
    char str[MAXSIZE];
    ReadOrder(c,s);
    strcpy(str,ReceFromBDS);
    str = strtok(str," \n");
    strcpy(Mynode.Time,str);
    str = strtok(NULL," \n");
    strcpy(Mynode.Name,str);
    str = strtok(NULL," \n");
    Mynode.parentcylinder = atoi(str);
    str = strtok(NULL," \n");
    Mynode.parentsector = atoi(str);
    str = strtok(NULL," \n");
    Mynode.c = atoi(str);
    str = strtok(NULL," \n");
    Mynode.s = atoi(str);
    str = strtok(NULL," \n");
    Mynode.used = atoi(str);
    str = strtok(NULL," \n");
    Mynode.flag = atoi(str);
    str = strtok(NULL," \n");
    Mynode.length = atoi(str);
    for (int i = 0;i < Mynode.used;++i)
    {     
        str = strtok(NULL," \n");
        Mynode.point[i][0] = atoi(str);
        str = strtok(NULL," \n");
        Mynode.point[i][1] = atoi(str);
    }
}
void ReadInodeData(InodeData & MyData,Inode Mynode)
{
    char str[MAXSIZE];

    ReadOrder(Mynode.point[0][0],Mynode.point[0][1]);
    strcpy(str,ReceFromBDS);
    str = strtok(str," \n");
    MyData.amount = atoi(str);
    for (int i = 0;i < MyData.amount;++i)
    {
        str = strtok(str," \n");
        strcpy(MyData.Name[i],str);

        str = strtok(str," \n");
        MyData.c[i] = atoi(str);
        str = strtok(str," \n");
        MyData.s[i] = atoi(str);
    }
}
void initial()
{
    char str[MAXSIZE];

    nowcylinder = 0;
    nowsector = 0;
    bzero(SendToBDS,sizeof(SendToBDS));
    strcat(SendToBDS,"R 0 0\n");
    Write(sockfd_BDS,SendToBDS,strlen(SendToBDS)); 
    Read(sockfd_BDS,ReceFromBDS,MAXSIZE);
    if (ReceFromBDS[0] != '0')
    {
        strcat(SendToBDS,"I\n");
        Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
        Read(sockfd_BDS,ReceFromBDS,MAXSIZE);
        strcpy(str,ReceFromBDS);

        str = strtok(str," \n");
        SuperBlock.cylinder = atoi(str);
        str = strtok(str," \n");
        SuperBlock.sector = atoi(str);
        SuperBlock.inodenum = (SuperBlock.cylinder * SuperBlock.sector) / 2;
        SuperBlock.datanum = SuperBlock.cylinder * SuperBlock.sector - SuperBlock.inodenum;

        SuperBlock.blockstart = SuperBlock.inodenum;
        SuperBlock.InodeBitMapLen = SuperBlock.inodenum / 8;
        if (SuperBlock.inodenum % 8 > 0) ++SuperBlock.InodeBitMapLen;
        for (int i = 0;i < SuperBlock.InodeBitMapLen;++i)
            InodeBitMap[i] = 0;

        SuperBlock.DataBitLen = SuperBlock.datanum / 8;
        if (SuperBlock.datanum % 8 > 0) ++SuperBlock.DataBitLen;
        for (int i = 0;i < SuperBlock.DataBitLen;++i)
            DataBitMap[i] = 0;

        SuperBlock.InodeBitBlock = SuperBlock.InodeBitMapLen / BLOCKSIZE;
        if (SuperBlock.InodeBitMapLen % BLOCKSIZE > 0) ++SuperBlock.InodeBitBlock;

        SuperBlock.DataBitBlock = SuperBlock.DataBitMapLen / BLOCKSIZE;
        if (SuperBlock.DataBitMapLen % BLOCKSIZE > 0) ++SuperBlock.DataBitBlock;

        SuperBlock.valid = '1';

        /*将BitMap 和superBlock的inode块标记为已用*/
        SuperBlock.usedinode = 1 + SuperBlock.InodeBitBlock + SuperBlock.DataBitBlock;
        SuperBlock.useddata = 0;
        for (int i = 0;i < SuperBlock.usedinode;++i)
        {
            tmp = i / 8;
            local = 8 - (i - (i / 8) * 8);
            InodeBitMap[tmp] = InodeBitMap[tmp] | (1 << (local - 1));
        }
        for (int i = 0;i < SuperBlock.usedinode;++i)
            Next(nowcylinder,nowsector);

        /*建立根目录*/
        SuperBlock.rootcylinder = nowcylinder;
        SuperBlock.rootsector = nowsector;
        ++SuperBlock.useddata;
        DataBitMap[0] = DataBitMap[0] | (1 << 7);

        time_t t;
        t = time(&t);
        strcpy(Root.Time,ctime(&t));
        strcpy(Root.Name,"/");
        Acylinder = 0;
        Asector = 0;
        GetBlock(RootData,Acylinder,Asector);
        Root.used = 1;
        Root.point[0][0] = Acylinder;
        Root.point[0][1] = Asector;
        Root.parentcylinder = 0;
        Root.parentsector = 0;
        Root.flag = 1;       /*这是个文件夹*/
        Root.c = nowcylinder;
        Root.s = nowsector;
        CurrentInode = Root;
        CurrentData = RootData;
    }
    else
    {
        strcpy(str,ReceFromBDS);
        str = strtok(str," \n");
        SuperBlock.valid = str[0];
        str = strtok(str," \n");
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
            Read(sockfd_BDS,ReceFromBDS,BLOCKSIZE);
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
            Write(sockfd_BDS,ReceFromBDS,BLOCKSIZE);
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
    return;
}

void Format()
{
    bzero(SendToBDS,sizeof(SendToBDS));
    strcpy(SendToBDS,"W 0 0 1 0\n");
    Write(sockfd_BDS,SendToBDS,BLOCKSIZE);
    initial();
}

bool EnterFile(Inode & Mynode,Inode & MyData,char *path)
{
    int k = -1;
    for (int i = 0;i < MyData.amount;++i)
        if (0 == strcmp(path,MyData.Name[i]))
        {
            k = i;
            break;
        }
    if (k < 0) 
    {
        HandleError("No such file exists\n");
        return 0;
    }
    c = MyData.c[k];
    s = MyData.s[k];
    ReadInode(Mynode,c,s);
    ReadInodeData(Mynode);
    if (0 == Mynode.flag)
    {
        HandleError("No such file exists\n");
        return 0;
    }
    return 1;    
}
void WriteBack(Inode & MyInode,InodeData & MyData)
{
    char info[MAXSIZE],tmp[BLOCKSIZE];

    bzero(SendToBDS,sizeof(SendToBDS));
    strcat(SendToBDS,"W ");
    tmp = itoa(MyInode.c);
    strcat(SendToBDS,tmp);
    strcat(SendToBDS," ");
    tmp = itoa(MyInode,s);
    strcat(SendToBDS,tmp);
    strcat(SendToBDS," ");
    bzero(info,sizeof(info));
    strcat(info,MyInode.Time);
    strcat(info," ");
    strcat(info,MyInode.Name);
    strcat(info," ");
    strcat(info,MyInode.parentcylinder);
    strcat(info," ");
    strcat(info,MyInode.parentsector);
    strcat(info," ");
    tmp = itoa(MyInode.c);
    strcat(info,tmp);
    strcat(info," ");
    tmp = itoa(MyInode.s);
    strcat(info,tmp);
    strcat(info," ");
    tmp = itoa(MyInode.used);
    strcat(info,tmp);
    strcat(info," ");
    tmp = itoa(MyInode.flag);
    strcat(info,tmp);
    strcat(info," ");
    tmp = itoa(MyINode.length);
    strcat(info,tmp);
    strcat(info," ");
    for (int i = 0;i < MyInode.used;++i)
    {
        tmp = itoa(MyInode.point[i][0]);
        strcat(info,tmp);
        strcat(info," ");
        tmp = itoa(MyInode.point[i][1]);
        strcat(info,tmp);
        strcat(info," ");
    }
    strcat(SendToBDS,itoa(strlen(info)));
    strcat(SendToBDS," ");
    strcat(SendToBDS,info);
    Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
    if (MyInode.flag == 1)
    {
        bzero(SendToBDS,sizeof(SendToBDS));
        bzero(info,sizeof(info));
        tmp = itoa(MyInode.point[0][0]);
        strcat(SendToBDS,"W ");
        strcat(SendToBDS,tmp);
        strcat(SendToBDS," ");
        tmp = itoa(MyInode.point[0][1]);
        strcat(SendToBDS,tmp);
        strcat(SendToBDS," ");
        tmp = itoa(MyInode.amount);
        strcat(info,tmp);
        strcat(info," ");
        for (int i = 0;i < MyData.amount;++i)
        {
            strcat(info,Name[i]);
            strcat(info," ");
            tmp = itoa(MyData.c[i]);
            strcat(info,tmp);
            strcat(info," ");
            tmp = itoa(MyData.s[i]);
            strcat(info,tmp);
            strcat(info," ");
        }
        strcat(SendToBDS,itoa(strlen(info)));
        strcat(SendToBDS," ");
        strcat(SendToBDS,info);
        Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
    }
    return;
}
int Changedir(char *path)
{
    bool Check;
    Inode TmpInode;
    InodeData TmpData;
    if (strlen(path) == 1 && path[0] == '.') return 1;
    else
    {
        if (path[0] == '/')
        {
            ReadInode(TmpInode,SuperBlock.rootcylinder,SuperBlock.rootsector);
            ReadInodeData(TmpData,TmpInode);
            path = strtok(&path[1],"/");
            while (path)
            {
                Check = EnterFile(TmpInode,TmpData,path);
                if (!Check) 
                {
                    HandleError("No such file or directory\n");
                    return 0;
                }
                path = strtok(NULL,"/");
            }
        }
        else
        {
            TmpInode = CurrentInode;
            TmpData = CurrentData;
            if (0 == strcmp(path,".."))
            {
                if (TmpIode.c == SuperBlock.rootcylinder && TmpInode.s == SuperBlock.rootsector)
                {
                    HandleError("No such file exists\n");
                    return 0;
                }
                ReadInode(TmpInode,TmpInode.parentcylinder,TmpInode.parentsector);
                ReadInodeData(TmpData,TmpInode);
            }
            else
            {
                path = strtok(path,"/");
                while (path)
                {
                    Check = EnterFile(tmpinode,tmpdata,path);
                    if (!Check)
                    {
                        HandleError("No such file or directory\n");
                        return 0;
                    }
                    path = strtok(NULL,"/");
                }
            }
        }
    }

    WriteBack(CurrentInode,CurrentData);
    CurrentInode = tmpinode;
    CurrentData = tmpdata;
    return 1;
}

void GetInode(Inode & NewInode,int Choice)
{
    int k;
    for (int i = 0;i < SuperBlock.InodeBitMapLen;++i) 
        if (InodeBitMap[i] == 0)
        {
            k = i;
            break;
        }
    InodeBitMap[k] = 1;
    NewInode.c = k / BLOCKSIZE;
    NewInode.s = k % BLOCKSIZE;
    NewInode.parentcylinder = CurrentInode.c;
    NewInode.parentsector = CurrentInode.s;
    NewInode.used = 0;
    NewInode.flag = Choice;
    NewInode.length = 0;
}

int Create(char *filename,int Choice)
{
    Inode NewInode;

    for (int i = 0; i < CurrentData.amount;++i)
        if (!strcmp(CurrentData.Name[i],filename))
        {
            HandleError("The file has existed\n");
            return 0;
        }

    if (!GetInode(NewInode,Choice))
    {
        HandleError("The inode is not enoutgh\n");
        return 0;
    }
    strcpy(CurrentData.Name[Current.amount],filename);
    CurrentData.c[CurrentData.amount] = NewInode.c;
    CurrentData.s[CurrentData.amount] = NewInode.s;
    ++CurrentData.amount;
    strcpy(NewInode.Name,filename);
    if (1 == Choice)
    {
        InodeData NewData;
        int Acylinder,Asector;
        if (!GetBlock(NewData,Acylinder,Asector))
        {
            HandleError("No enough DataBlock\n");
            return 0;
        };
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

    strcpy(filename,&path[k + 1]);
    TempInode = CurrentInode;
    TempData = CurrentData;
    if (filename == NULL || !Changedir(path))
    {
        HandleError("No such file or directory\n");
        return;
    };
    if (!Create(filename,Choice))  
    {
        HandleError("No such file or directory\n");
        if (k > -1)
        {
            CurrentInode = TempInode;
            CurrentData = TempData;
        }
        return;
    }
}
void List(char *argument)
{
    char str[10];
    Inode TmpInode;
    if (argument[0] == '0')
    {
        bzero(SendToFC,sizeof(SendToFC));
        for (int i = 0;i < CurrentData.amount;++i)
        {
            strcat(SendToFC,CurrentData.Name[i]);
            strcat(SendToFC,"\n");
        }
        Write(client_fd,SendToFC,strlen(SendToFC));
    }
    else
    {
        bzero(SendToFC,sizeof(SendToFC));
        for (int i = 0;i < CurrentData.amount;++i)
        {
            strcat(SendToFC,Name[i]);
            strcat(SendToFC," created time: ");
            ReadInode(TmpInode,CurrentData.c[i],CurrentData.s[i]);
            SendToFC = strcat(SendToFC,TmpInode.Time);
            SendToFC = strcat(SendToFC," ");
            str = itoa(TmpInode.length);
            SendToFC = strcat(SendToFC,str);
            SendToFC = strcat(SendToFC,"\n");
        }
    }
    return ;
}

char * ReadData(char *str,Inode Node)
{
    int c,s;
    char tmp[10];
    bzero(str,sizeof(str));
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
    char str[MAXSIZE];
    if (1 == CurrentInode.flag) return 0;
    for (int i = 0;i < CurrentData.amount;++i)
        if (!strcmp(filename,CurrentData.Name[i]))
        {
            k = i;
            break;
        }
    if (k < 0) 
    { 
        HandleError("No such file\n");
        return 0;
    }
   ReadInode(TmpInode,CurrentData.c[k],CurrentData.s[k]);
    if (1 == TmpInode.flag)
    {
        HandleError("No such file\n");
        return 0;
    }
    bzero(SendToFC,sizeof(SendToFC));
    SendToFC = strcat(SendToFC,"0 ");
    str = itoa(TmpInode.length);
    SendToFC = strcat(SendToFC,str);
    SendToFC = strcat(SendToFC," ");
    str = ReadData(str,TmpInode);
    SendToFC = strcat(SendToFC,str);
    Write(client_sockfd,SendToFC,strlen(SendToFC));
    return 1;
}
void Catchfile(char *path)
{
    int k = -1;
    char filename[BLOCKSIZE];

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
    if (filename == NULL || !Changedir(path))
    {
        HandleError("No such file or directory\n");
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
    int len,k,Acylinder,Asector;
    Inode TmpNode;

    filename = strtok(NULL," \n");
    tmp = strtok(NULL," \n");
    len = atoi(tmp);
    data = strtok(NULL," \n");
    k = -1;
    for (int i = 0;i < CurrentData.amount;++i)
        if (0 == strcmp(filename,Name[i]))
        {
            k = i;
            break;
        }
    if (k < 0) 
    {
        HandleError("No such file exists\n");
        return;
    }

    ReadInode(TmpInode,CurrentData.c[k],CurrentData.s[k]);
    if (1 == TmpNode.flag)
    {
        HandleError("No such file exist\n");
        return;
    }
    else
    {
        TmpNode.length = len;
        TmpNode.used = len / BLOCKSIZE;
        if (0 != len % BLOCKSIZE) ++TmpNode.used;
        for (int i = 0;i < TmpNode.used;++i)
        {
            if (point[i][0] == 0 && point[i][1] == 0)
            {
                InodeData NewData;
                GetBlock(NewData,Acylinder,Asector);
                CurrentInode.point[i][0] = Acylinder;
                CurrentInode.point[i][1] = Asector;
            }
            bzero(SendToBDS,sizeof(SendToBDS));
            SendToBDS = strcat(SendToBDS,"W ");
            tmp = itoa(Acylinder);
            SendToBDS = strcat(SendToBDS,tmp);
            SendToBDS = strcat(SendToBDS," ");
            tmp = itoa(Asector);
            SendToBDS = strcat(SendToBDS,tmp);
            SendToBDS = strcat(SendToBDS," ");
            if (i < TmpNode.used - 1) tmp = itoa(BLOCKSIZE);
            else tmp = itoa(len % BLOCKSIZE);
            SendToBDS = strcat(SendToBDS,tmp);
            SendToBDS = strcat(SendToBDS," ");
            data = &data[i * BLOCKSIZE]
                tmp = strncpy(tmp,data,BLOCKSIZE);
            SendToBDS = strcat(SendToBDS,tmp);
            Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
        }
    }
    return ;
}


void Append(char *path)
{
    char *filename,*tmp,*data;
    int len,k;

    filename = strtok(NULL," \n");
    tmp = strtok(NULL," \n");
    len = atoi(tmp);
    data = strtok(NULL," \n");
    k = -1;
    for (int i = 0;i < CurrentData.amount;++i)
        if (0 == strcmp(filename,Name[i]))
        {
            k = i;
            break;
        }
    if (k < 0) 
    {
        HandleError("No such file exists\n");
        return;
    }
    Inode TmpNode;
    TmpNode = ReadInode(CurrentData.c[k],CurrentData.s[k]);
    if (1 == TmpNode.flag)
    {
        HandleError("No such file exist\n");
        return;
    }
    else
    {
        tmplen = TmpNode.length + len;
        bzero(tmpdata,sizeof(tmpdata));
        for (int i = 0;i < TmpNode.used;++i)
        {
            bzero(SendToBDS,sizeof(SendToBDS));
            strcpy(SendToBDS,"R ");
            tmp = itoa(point[i][0]);
            strcpy(SendToBDS,tmp);
            tmp = itoa(point[i][1]);
            strcpy(SendToBDS,tmp);
            Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
            Read(sockfd_BDS,ReceFromBDS,MAXSIZE);
            tmpdata = strcpy(tmpdata,ReceFromBDS);
        }
        bzero(tmp,sizeof(tmp));
        strcpy(tmp,filename);
        strcat(tmp," ");
        strcat(tmp,itoa(tmplen));
        strcat(tmp," ");
        strcat(tmp,tmpdata);
        WriteData(tmpdata);
    }
    return;
}

void Exit()
{
    char *info[BLOCKSIZE],*tmp[BLOCKSIZE];

    /*SuperBlock*/
    bzero(info,sizeof(info));
    info[0] = valid;

    tmp = itoa(SuperBlock.cylinder);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.sector);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.inodenum);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.datanum);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.InodeBitMapLen);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.DataBitMapLen);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.InodeBitBlock);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.DataBitBlock);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.usedinode);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.useddata);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.rootcylinder);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.rootsector);
    info = strcat(info,tmp);
    info = strcat(info," ");

    tmp = itoa(SuperBlock.blockstart);
    info = strcat(info,tmp);

    len = strlen(info);
    tmp = itoa(len);
    tmp = strcat(tmp," ");

    bzero(SendToBDS,sizeof(SendToBDS));
    SendToBDS = strcat(SendToBDS,"W 0 0 ");
    SendToBDS = strcat(SendToBDS,tmp);
    SendToBDS = strcat(SendToBDS,info);
    Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
    /*InodeBitMap*/
    cylinder = 0;
    sector = 0;
    for (int i = 0;i < SuperBlock.InodeBitBlock;++i)
    {
        Next(cylinder,sector);
        strncpy(str,InodeBitMap[i * BLOCKSIZE],BLOCKSIZE);
        WriteOrder(cylinder,sector,str);
    }
    /*DataBitMap*/
    for (int i = 0;i < SuperBlock.DataBitBlock;++i)
    {
        Next(cylinder,sector);
        strncpy(str,DataBitMap[i * BLOCKSIZE],BLOCKSIZE);
        WriteOrder(cylinder,sector,str);
    }

    /*CurrentInode*/
    WriteBack(CurrentInode,CurrentData);
    return;
}
