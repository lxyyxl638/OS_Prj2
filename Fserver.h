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

void initial()
{
    char str[MAXSIZE];

    bzero(SendToBDS,sizeof(SendToBDS));
    strcat(SendToBDS,"R 0 0\n");
    Write(sockfd_BDS,SendToBDS,strlen(SendToBDS)); 
    Read(sockfd_BDS,ReceFromBDS,MAXSIZE);

    if (ReceFromBDS[0] != '1')
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
        GetBlock(RootData);
        Root.used = 1;
        Root.point[Root.used][0] = Acylinder;
        Root.point[Root.used][1] = Asector;
        Root.parentcylinder = 0;
        Root.parentsector = 0;
        Root.flag = Folder;
        Root.c = nowcylinder;
        Root.s = nowcylinder;
    }
    else
    {
        strcpy(str,ReceFromBDS);
        str = strtok(str," ");
        SuperBlock.valid = str[0];
        str = strtok(str," ");
        SuperBlock.cylinder = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.sector = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.inodenum = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.datanum = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.InodeBitMapLen = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.DataBitMapLen = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.InodeBitBlock = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.DataBitBlock = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.usedinode = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.useddata = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.rootcylinder = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.rootsector = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.blockstart = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.c = atoi(str);
        str = strtok(NULL," ");
        SuperBlock.s = atoi(str);

        k = 0;
        for (int i = 0;i < SuperBlock.InodeBitBlock;++i)
        {
            Next(nowcylinder,nowsector);
            ReadOrder(nowcylinder,nowsector);
            Write(sockfd_BDS,ReceFromBDS,BLOCKSIZE);
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
        LoadInode(Root,SuperBlock.cylinder,SuperBlock.sector);
    }
    CurrentInode = Root;
    CurrentData = RootData;
    return;
}

void Format()
{
    bzero(SendToBDS,sizeof(SendToBDS));
    strcpy(SendToBDS,"W 0 0 1 0");
    Write(sockfd_BDS,SendToBDS,BLOCKSIZE);
    initial();
}

int Changedir(char *path)
{
    if (strlen(path) == 1 && path[0] == '.') return 1;
    else
    {
        if (path[0] == '/')
        {
            tmpinode = Root;
            tmpdata = RootData;
            path = strtok(&path[1],"/");
            while (path)
            {
                Check = EnterFile(tmpinode,tmpdata,path,tmpcylinder,tmpsector);
                if (!Check) 
                {
                    HandleError("No such file or directory\n");
                    return 0;
                }
                Change(tmpinode,tmpdata,tmpcylinder,tmpsector);
                path = strtok(NULL,"/");
            }
        }
        else
        {
            tmpinode = CurrentInode;
            tmpdata = CurrentData;
            path = strtok(path,"/");
            while (path)
            {
                Check = EnterFile(tmpinode,tmpdata,path,tmpcylinder,tmpsector);
                if (!Check)
                {
                    HandleError("No such file or directory\n");
                    return 0;
                }
                Change(tmpinode,tmpdata,tmpcylinder,tmpsector);
                path = strtok(NULL,"/");
            }
        }
    }

    WriteBack(CurrentInode,CurrentData);
    CurrentInode = tmpinode;
    CurrentData = tmpdata;
    return 1;
}

int Create(char *filename,int Choice)
{
    Inode NewInode;

    for (int i = 0; i < CurrentData.amount;++i)
        if (!strcmp(Name[i],filename))
        {
            HandleError("The file has existed\n");
            return 0;
        }

    if (!GetInode(NewInode))
    {
        HandleError("The inode is not enoutgh\n");
        return 0;
    }
    ++CurrentData.amount;
    strcpy(CurrentData.Name[Current.amount],filename);
    CurrentData.c[CurrentData.amount] = NewInode.Acylinder;
    CurrentData.s[CurrentData.amount] = NewInode.Asector;
    strcpy(NewInode.Name,filename);
    NewInode.parentcylinder = CurrentInode.c;
    NewInode.parentsector = CurrentInode.s;
    NewInode.used = 0;
    NewInode.flag = Choice;
    if (1 == Choice)
    {
        InodeData NewData;
        if (!GetBlock(NewData,1))
        {
            HandleError("No enough DataBlock\n");
            return 0;
        };
       NewInode[0][0] = NewData.c;
       NewInode[0][1] = NewData.s;
    }
    WriteBack(CurrentInode,CurrentData);
    WriteBack(NewInode,NewData);
    return 1;
}

void Createfile(char *path,int Choice)
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
    if (!Create(filename,Choice))  
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

int Remove(char *filename,int Choice)
{
    Inode NewInode;

    for (int i = 0; i < CurrentData.amount;++i)
        if (!strcmp(Name[i],filename))
        {
            HandleError("The file has existed\n");
            return 0;
        }

    if (!GetInode(NewInode))
    {
        HandleError("The inode is not enoutgh\n");
        return 0;
    }
    ++CurrentData.amount;
    strcpy(CurrentData.Name[Current.amount],filename);
    CurrentData.c[CurrentData.amount] = NewInode.Acylinder;
    CurrentData.s[CurrentData.amount] = NewInode.Asector;
    strcpy(NewInode.Name,filename);
    NewInode.parentcylinder = CurrentInode.c;
    NewInode.parentsector = CurrentInode.s;
    NewInode.used = 0;
    NewInode.flag = Choice;
    if (1 == Choice)
    {
        InodeData NewData;
        if (!GetBlock(NewData,1))
        {
            HandleError("No enough DataBlock\n");
            return 0;
        };
       NewInode[0][0] = NewData.c;
       NewInode[0][1] = NewData.s;
    }
    WriteBack(CurrentInode,CurrentData);
    WriteBack(NewInode,NewData);
    return 1;
}
void Removefile(char *path,int Choice)
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
    if (!Remove(filename,Choice))  
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

void List(char *argument)
{
    char str[10];
    if (argument[0] == '0')
    {
        bzero(SendToFC,sizeof(SendToFC));
        for (int i = 0;i < CurrentData.amount;++i)
         {
            SendToFC = strcat(SendToFC,CurrentData.Name[i]);
            SendToFC = strcat(SendToFC,"\n");
         }
        Write(sockfd_FC,SendToFC,strlen(SendToFC));
    }
    else
    {
        bzero(SendToFC,sizeof(SendToFC));
        for (int i = 0;i < CurrentData.amount;++i)
        {
            SendToFC = strcat(SendToFC,Name[i]);
            SendToFC = strcat(SendToFC," created time: ");
            Inode TmpInode = ReadInode(CurrentData.c[i],CurrentData.s[i]);
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
       c = point[i][0];
       s = point[i][1];
       bzero(SendToBDS,sizeof(SendToBDS));
       strcat(SendToBDS,"R ");
       tmp = itoa(c);
       strcat(SendToBDS,tmp);
       strcat(SendToBDS," ");
       tmp = itoa(s)
       strcat(SendToBDS,tmp);
       strcat(SendToBDS,"\n");
       Write(sockfd_BDS,SendToBDS,strlen(SendToBDS));
       Read(sockfd_BDS,ReceFromBDS,BLOCKSIZE);
       strcat(str,ReceFromBDS);
    }
    return str;
}
int Catch(char *filename)
{
    int k = -1;
    Inode TmpInode;
    char str[MAXSIZE];
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
    TmpInode = ReadInode(CurrentData.c[k],CurrentData.s[k]);
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
        TmpNode.length = len;
        TmpNode.used = len / BLOCKSIZE;
        if (0 != len % BLOCKSIZE) ++TmpNode.used;
        for (int i = 0;i < TmpNode.used;++i)
         {
            if (point[i][0] == 0 && point[i][1] == 0)
             {
                 GetBlock(Acylinder,Asector);
                 point[i][0] = Acylinder;
                 point[i][1] = Asector;
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
