#include "user.h"

#define MAXNUM 35
int fork1();
void process(int* pre_pipe);
int
main(int argc, char* argv[])
{
    int data[MAXNUM];
    for(int i=0;i<MAXNUM;i++) {
        data[i]=i+2;
    }
    int init_pipe[2];
    pipe(init_pipe);
    if(fork1()>0) {
        close(init_pipe[0]);
        write(init_pipe[1],data,MAXNUM*sizeof(int));
        close(init_pipe[1]);
        wait(0);
    }else {
        process(init_pipe);
    }
    exit(0);
}

int
fork1() 
{
    int pid=fork();
    if(pid<0) {
        fprintf(2,"fork error\n");
        exit(1);
    }
    return pid;
}

void
process(int* pre_pipe)
{
    close(pre_pipe[1]);  // 只从上一个管道接收数据
    // last process
    int prime=0;
    if(read(pre_pipe[0],&prime,sizeof(int))==0) {
        exit(0);
    }
    printf("prime %d\n",prime);
    int data[MAXNUM];
    int tmp=0,cnt=0;
    while(read(pre_pipe[0],&tmp,sizeof(int))) {
        if(tmp%prime) {
            data[cnt++]=tmp;
        }
    }
    close(pre_pipe[0]);

    int next_pipe[2];
    if(pipe(next_pipe)<0) {
        fprintf(2,"pipe error here\n");
        exit(1);
    }
    if(fork1()>0) {
        close(next_pipe[0]);
        write(next_pipe[1],data,cnt*sizeof(int));
        close(next_pipe[1]);
        wait(0);
        exit(0);
    }else {
        process(next_pipe);
    }
}