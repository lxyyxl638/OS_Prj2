#include "Fserver.h"
struct SB
{
    int cylinder,sector,inodenum,datanum,InodeBitMapLen,DataBitMapLen,InodeBitBlock,DataBitBlock,usedinode,useddata,rootcylinder,rootsector,blockstart;
    char valid;
};

struct Inode
{
    char Time[BLOCKSIZE],Name[BLOCKSIZE];
    int point[12][2];
    int parentcylinder,parentsector,c,s,used,flag;
}
struct InodeData
{
    char Name[BLOCKSIZE][BLOCKSIZE];
    int c[BLOCKSIZE],s[BLOCKSIZE],amount;
};
char DataBitMap[MAXSIZE],InodeBitMap[MAXSIZE];
SB SuperBlock;
Inode CurrentInode,Root;
InodeData CurrentData,RootData;
int main(int argc,char* argv[])
{
    int Port_BDS = atoi(argv[2]);
    int Port_FC = atoi(argv[3]);

    sockfd_BDS = Socket();
    sockfd_FC = Socket();

    struct sockaddr_in servself_addr,client_addr,servBDS_addr;
    struct hostent *host;
    int nread;
    socklen_t len;

    Bind(sockfd_FC,servself_addr,Port_FC);
    Listen(sockfd_FC,5);
    printf("The FS is listening\n");

    bzero(&servBDS_addr,sizeof(servBDS_addr));
    servBDS_addr.sin_family = AF_INET;
    host = gethostbyname(argv[1]);
    memcpy(&servBDS_addr.sin_addr.s_addr,host->h_addr,host->h_length);
    servBDS_addr.sin_port = htons(Port_BDS);
    connect(sockfd_BDS,(struct sockaddr*)&servBDS_addr,sizeof(servBDS_addr));
    
    initial();
    while (1)
    {
       len = sizeof(client_addr);
       client_sockfd = accept(sockfd_FC,(struct sockaddr *) & client_addr, &len);
       printf("Connect successfully\n");
       while (1)
       {
           Read(client_sockfd,ReceFromFC,MAXSIZE);
           strcpy(str,ReceFromFC);
           str = strtok(NULL," \n");
           if (0 == strcmp(str,"f")) Format();
           else if (0 == strcmp(str,"mk")) 
           {
               str = strtok(NULL," \n")
               Createfile(str,0);
           }
           else if (0 == strcmp(str,"mkdir")) 
           {
               str = strtok(NULL," \n");
               Createfile(str,1);
           }
           else if (0 == strcmp(str,"rm")) Removefile();
           else if (0 == strcmp(str,"cd")) 
           {
               str = strtok(NULL," \n");
               Changedir(str);
           }
           else if (0 == strcmp(str,"rmdir d")) Deletedir();
           else if (0 == strcmp(str,"ls")) List();
           else if (0 == strcmp(str,"cat")) Catch();
           else if (0 == strcmp(str,"w")) WriteData();
           else if (0 == strcmp(str,"a")) Append();
           else if (0 == strcmp(str,"exit")) Exit();
       }
       Close(client_sockfd);
    }
    Close(sockfd_BDS);
    Close(sockfd_FC);
}
