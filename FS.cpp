#include "Fserver.h"

int main(int argc,char* argv[])
{
    char *str = new char[MAXSIZE],*p;
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
        client_sockfd = accept(sockfd_FC,(struct sockaddr *) &client_addr, &len);
        printf("Connect successfully\n");

        bzero(SendToFC,sizeof(SendToFC));
        strcat(SendToFC,CurrentInode.Name);
        strcat(SendToFC," $ ");
        Write(client_sockfd,SendToFC,strlen(SendToFC));
        while (1)
        {
            bzero(ReceFromFC,sizeof(ReceFromFC));
           int n = Read(client_sockfd,ReceFromFC,MAXSIZE);
            Write(STDOUT_FILENO,ReceFromFC,strlen(ReceFromFC));
           // Show(CurrentInode,CurrentData);
            strcpy(str,ReceFromFC);
            p = str;
            str = strtok(str," \n");
            if (0 == strcmp(str,"f")) Format();
            else if (0 == strcmp(str,"mk"))  /*Create a file*/ 
            {
                str = strtok(NULL," \n");
                Createfile(str,0);
            }
            else if (0 == strcmp(str,"mkdir")) /*Create a folder*/
            {
                str = strtok(NULL," \n");
                Createfile(str,1);
            }
            else if (0 == strcmp(str,"rm")) /*Remove a file*/
            {
                str = strtok(NULL," \n");
                 Removefile(str,0);
            }
            else if (0 == strcmp(str,"cd")) /*Change path*/
            {
                str = strtok(NULL," \n");
                Changedir(str);
                HandleError("cdok!\n");
            }
            else if (0 == strcmp(str,"rmdir")) 
            {
                str = strtok(NULL," \n"); /*Remove a folder*/
                Removefile(str,1);
            }
            else if (0 == strcmp(str,"ls")) 
            {
                str = strtok(NULL," \n");
                List(str);
            }
            else if (0 == strcmp(str,"cat")) /*Catch a file*/ 
            {
                str = strtok(NULL," \n");
                Catchfile(str);
                printf("Catch ok!\n");
            }
            else if (0 == strcmp(str,"w")) 
            {
                str = strtok(NULL,"\n");
                WriteData(str);
            }
            else if (0 == strcmp(str,"a")) 
            {
                str = strtok(NULL,"\n");
                Append(str);
            }
            else if (0 == strcmp(str,"exit")) 
                Exit();
            else 
            {
                HandleError("2:Unavailable command\n");
            }
            bzero(SendToFC,sizeof(SendToFC));
            strcat(SendToFC,CurrentInode.Name);
            strcat(SendToFC," $ ");
            Write(client_sockfd,SendToFC,strlen(SendToFC));
        }
        Close(client_sockfd);
    }
    Close(sockfd_BDS);
    Close(sockfd_FC);
}
