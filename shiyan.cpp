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

int main()
{
    char *str = new char[10];
    strcpy(str,"ABCAAJKDF");
    str = strtok(str,"A");
    while (str)
    {
        printf("%s\n",str);
        str = strtok(NULL,"A");
    }
    return 0;
}
