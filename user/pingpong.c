#include "user.h"

int 
main(int argc, char* argv[])
{
    int parent_pipe[2],child_pipe[2];
    if(pipe(parent_pipe)<0 || pipe(child_pipe)<0) {
        fprintf(2,"pipe error\n");
        exit(1);
    }
    int pid=fork();
    if(pid>0) {
        write(parent_pipe[1],"ping",4);
        close(parent_pipe[1]);
        wait((int*) 0);  // 等子进程结束
        char buf[5];
        read(child_pipe[0],buf,sizeof(buf));
        printf("%d: received %s\n",getpid(),buf);
    }else if(pid==0) {
        write(child_pipe[1],"pong",4);
        close(child_pipe[1]);
        char buf[5];
        read(parent_pipe[0],buf,sizeof(buf));
        printf("%d: received %s\n",getpid(),buf);
    }else {
        printf("fork error\n");
    }
    exit(0);
}