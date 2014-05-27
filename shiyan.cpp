#include <cstring>
#include <queue>
#include <cstdio>
#include <cstdlib>

struct Node
{
    char C,a;
    int b;
};
void task(char p[],Node &mynode)
{ 
    char *task_ptr = NULL;

    p = strtok_r(p," ",&task_ptr);
    printf("%s\n",p);
    p = strtok_r(NULL," ",&task_ptr);
    printf("%s\n",p);
    p = strtok_r(NULL," ",&task_ptr);
    printf("%s\n",p);
    mynode.b = atoi(p); 
        
}
int main()
{
    char str[] = "W 4 5\nR 2 4\n";
    char del[] = "\n";
    char *p = new char[100];
    char *main_ptr = NULL;
    std::queue <char *> Myqueue;
    Node mynode;

    p = strtok_r(str,del,&main_ptr);
    while (p != NULL)
    {
        printf("str: %s\n",str);
        printf("%s\n",p);
        task(p,mynode);
        printf("%c %c\n",mynode.C,mynode.a);
       //Myqueue.push(p);
        p = strtok_r(NULL,del,&main_ptr);
    }
   /* printf("\n");
    while (!Myqueue.empty())
    {
        printf("%s\n",Myqueue.front());
        Myqueue.pop();
    }
*/
    return 0;
}
