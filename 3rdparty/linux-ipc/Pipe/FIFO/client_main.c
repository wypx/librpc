/**
 *project：利用FIFO实现客户端-服务器客户端(无亲缘关系demo)
 *author：Xigang Wang
 *email：wangixgang2014@gmail.com
 *desc:管道只能在有亲缘关系的进程间进行通信，FIFO可以在无亲缘关系的进程间通信,但本程序是有亲源关系的
 */
#include "fifo.h"

#define FIFO1 "/tmp/fifo.1"
#define FIFO2 "/tmp/fifo.2"
#define SERV_FIFO "/tmp/fifo.serv"

void client(int readfd, int writefd)
{
  size_t  len, n;
  char  buff[MAXLINE];

  fgets(buff, MAXLINE, stdin);
  len = strlen(buff);   /* fgets() guarantees null byte at end */
  if (buff[len-1] == '\n')
    len--;        /* delete newline from fgets() */

  write(writefd, buff, len);  /* 4write pathname to IPC channel */

    /* 4read from IPC, write to standard output */
  while ( (n = read(readfd, buff, MAXLINE)) > 0)
    write(STDOUT_FILENO, buff, n);
}
int main(int argc, char *argv[])
{
  int readfd, wirtefd;
  writefd = open(FIFO1, O_RDONLY, 0);
  readfd = open(FIFO2, O_WRONLY, 0);
  
  client(readfd, writefd)
  close(readfd);
  close(writefd);
  unlink(FIFO1);
  unlink(FIFO2);
  exit(0);
  return 0;
}
