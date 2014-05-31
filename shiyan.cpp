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

void task(char *str)
{
   while (str)
   {
        str = strtok(NULL,"C");
        printf("%s ",str);
   }
}
int main()
{
    char *str = new char[20];
    strcpy(str,"ABCAAJKCDF");
    str = strtok(str,"C");
    printf("%s ",str);
    task(str);
    return 0;
}
