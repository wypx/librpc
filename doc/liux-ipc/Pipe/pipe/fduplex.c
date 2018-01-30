/**
 *project:全双工管道测试
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */
#include "unpipc.h"

int main(int argc, char *argv[])
{
  int fd[2], n;
  char c;
  pid_t childpid;
  
  Pipe(fd);/*创建一个全双工管道*/
  if( (childpid = Fork()) == 0){
    sleep(3);
    if( (n = Read(fd[0], &c, 1)) != 1)
      err_quit("child: read returned %d", n);
    printf("child read %c\n", c);
    Write(fd[0], "c", 1);
    exit(0);
  }

  Write(fd[1], "p", 1);
  if( (n = Read(fd[1], &c, 1)) != 1)
    err_quit("parent: read returned %d", n);
  printf("parent read %c\n", c);
  exit(0);
}
