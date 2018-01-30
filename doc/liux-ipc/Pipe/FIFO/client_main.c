/**
 *project：利用FIFO实现客户端-服务器客户端(无亲缘关系demo)
 *author：Xigang Wang
 *email：wangixgang2014@gmail.com
 *desc:管道只能在有亲缘关系的进程间进行通信，FIFO可以在无亲缘关系的进程间通信,但本程序是有亲源关系的
 */
#include "fifo.h"

void client(int, int);

int main(int argc, char *argv[])
{
  int readfd, wirtefd;
  
  writefd = Open(FIFO1, O_RDONLY, 0);
  readfd = Open(FIFO2, O_WRONLY, 0);
  
  client(readfd, writefd)
    
  Close(readfd);
  Close(writefd);
  
  Unlink(FIFO1);
  Unlink(FIFO2);
  exit(0);
  return 0;
}
