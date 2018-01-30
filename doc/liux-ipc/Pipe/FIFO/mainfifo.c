/**
 *project：利用FIFO实现客户端-服务器程序
 *author：Xigang Wang
 *email：wangixgang2014@gmail.com
 *desc:管道只能在有亲缘关系的进程间进行通信，FIFO可以在无亲缘关系的进程间通信,但本程序是有亲源关系的
 */
#include "unpipc.h"

#define FIFO1 "/tmp/fifo.1"
#define FIFO2 "/tmp/fifo.2"

void client(int, int);
void server(int, int);

int main(int argc, char *argv[])
{
  int readfd, writefd;
  pid_t childpid;
  
  /*在/tmp文件系统下创建两个FIFO， FILE_MODE default unpipc.h*/
  if( (mkfifo(FIFO1, FILE_MODE) < 0) && (errno != EEXIST))
    err_sys("can't create %s", FIFO1);
  if( (mkfifo(FIFO2, FILE_MODE) < 0) && (errno != EEXIST)){
    unlink(FIFO1);
    err_sys("can't create %s", FIFO2);
  }

  /*调用fork创建一个子进程，打开FIFO1来读， 打开FIFO2来写*/
  if( (childpid = Fork()) == 0){
    readfd = Open(FIFO1, O_RDONLY, 0);
    writefd = Open(FIFO2, O_WRONLY, 0);
    /*调用服务器程序*/
    server(readfd, writefd);
    exit(0);
  }

  /*父进程 打开FIFO1来写， 打开FIFO2来读*/
  writefd = Open(FIFO1, O_WRONLY, 0);
  readfd = Open(FIFO2, O_RDONLY, 0);
  /*调用客户端程序*/
  client(readfd, writefd);
  
  /*父进程在子进程没有终止时， 不阻塞*/
  Waitpid(childpid, NULL, 0);  
  Close(readfd);
  Close(writefd);
  
  /*删除文件系统上FIFO1 和 FIFO2 的管道名*/
  Unlink(FIFO1);
  Unlink(FIFO2);
  exit(0);
}
