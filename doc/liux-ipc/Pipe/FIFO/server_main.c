/**
 *project：利用FIFO实现客户端-服务器程序服务端(无亲缘关系demo)
 *author：Xigang Wang
 *email：wangixgang2014@gmail.com
 *desc:管道只能在有亲缘关系的进程间进行通信，FIFO可以在无亲缘关系的进程间通信,但本程序是有亲源关系的
 */

#include “fifo.h”

void server(int, int);

int main(int argc, char *argv[])
{
  int readfd, writefd;
  
  /*创建两个FIFO*/
  if( (mkfifo(FIFO1, FILE_MODE) < 0) && (errno != EEXIST))
    err_sys("can't create %s", FIFO1);
  if( (mkfifo(FIFO2, FILE_MODE) < 0) &&( errno != EEXIST)){
    unlink(FIFO1);
    err_sys("can't create %s", FIFO2);
  }

  readfd = Open(FIFO1, O_RDONLY, 0);
  writefd = Open(FIFO2, O_WRONLY, 0);
  
  server(readfd, writefd);
  exit(0);
}

