/*For my superY */
/*Never Give up*/

#include "Iserver.h"
clock_t start,end;
bool Permisson; 
int main(int argc,char* argv[])
{
    char *filename = argv[1];
    int port = atoi(argv[5]);
    int amount = atoi(argv[6]);
    char *schedule = argv[7];
    int fd = Open(filename);
    long filesize;
    int result,sockfd;
    struct sockaddr_in serv_addr;

    struct sockaddr_in client_addr;
    int nread;
    socklen_t len;
    char str[MAXSIZE + MAXSIZE];
    char *out_ptr = NULL,*p = NULL;
    Node Mynode;

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
        switch (schedule[0])
        {
            case 'F':pthread_create(&tid,NULL,FCFS,NULL);break;
            case 'S':pthread_create(&tid,NULL,SSTF,NULL);break;
            case 'C':pthread_create(&tid,NULL,C_LOOK,NULL);break;
        }
        flag = false;
        start = clock();
        while (1)
        {
            bzero(recebuf,sizeof(recebuf));
            nread = Read(client_sockfd,recebuf,MAXSIZE);
            if (0 == nread)
            {
                printf("The other size has been closed.\n");
                pthread_join(tid,NULL);
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
                init(p,Mynode); // change the command-line to Mynode
                switch (schedule[0])
                {
                    case 'F': while (CacheFCFS.size() > amount);
                                  pthread_mutex_lock(&mutex);
                                  CacheFCFS.push(Mynode);
                                  pthread_mutex_unlock(&mutex);
                              break;
                    case 'S': while (CacheSSTF.size() > amount);
                                  pthread_mutex_lock(&mutex);
                                  CacheSSTF.push(Mynode);
                                  pthread_mutex_unlock(&mutex);
                              break;
                    case 'C': while (CacheC_LOOK.size() > amount);
                                  pthread_mutex_lock(&mutex);
                                  CacheC_LOOK.push(Mynode);  
                                  pthread_mutex_unlock(&mutex);
                              break;
                }
                if (0 == strcmp(p,"EXIT"))
                {
                    pthread_join(tid,NULL);
                    printf("ok\n");
                    while(!flag);
                    break;
                }

                Permisson = false;
               for (int i = 0;i < strlen(out_ptr);++i)
                if (out_ptr[i] == '\n')
                {
                    Permisson = true;
                    break;
                }

               if (!Permisson && (strlen(out_ptr) > 0)) break; 
                 else p = strtok_r(NULL,"\n",&out_ptr);
               
            }
          if (flag) break;
        }
        end = clock();
        float time = (float)(end - start)/CLOCKS_PER_SEC;
        printf("Time is %f\n",time);
        Close(client_sockfd);
    }
    Close(sockfd);
    return 0;
}

