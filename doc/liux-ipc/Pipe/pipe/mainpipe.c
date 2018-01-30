/**
 *project：利用管道实现客户-文件服务器
 *author：Xigang Wang
 *email:wangxigang2014@gmail.com
 */
#include "unpipc.h"

void client(int, int), server(int, int);

int main(int argc, char *argv[])
{
  int pipe1[2], pipe2[2];
  pid_t childpid;
  /*创建两个管道， fork*/
  Pipe(pipe1);
  Pipe(pipe2);

  if( (childpid = Fork()) == 0){/*child process*/
    Close(pipe1[1]);
    Close(pipe2[0]);

    server(pipe1[0], pipe2[1]);
    exit(0);
  }
  
  /*父进程*/
  Close(pipe1[0]);
  Close(pipe2[1]);
 
  client(pipe2[0], pipe1[1]);
  /*为子进程waitpid*/
  Waitpid(childpid, NULL, 0);
  exit(0);
}


