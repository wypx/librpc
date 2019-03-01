/**
 *project：利用FIFO实现客户端-服务器程序服务端(无亲缘关系demo)
 *author：Xigang Wang
 *email：wangixgang2014@gmail.com
 *desc:管道只能在有亲缘关系的进程间进行通信，FIFO可以在无亲缘关系的进程间通信,但本程序是有亲源关系的
 */

#include “fifo.h”

void server(int readfd, int writefd)
{
  int fd;
  ssize_t n;
  char buff[MAXLINE+1];
  
  /*从管道读路径名*/
  if( (n = read(readfd, buff, MAXLINE)) == 0)
    err_quit("end-of -file while reading pathname");
  buff[n] = '\0';
  
  if( (fd = open(buff, O_RDONLY)) < 0){/*打开文件处理错误*/
    snprintf(buff+n, sizeof(buff) - n, ":can't open, %s\n", 
       strerror(errno));
    n = strlen(buff);
    write(writefd, buff, n);
  }else{
    while( (n = read(fd, buff, MAXLINE)) > 0)/*把文件复制到管道*/
      write(writefd, buff, n);
    close(fd);
  }
}

int main(int argc, char *argv[])
{
  int readfd, writefd;
  /*创建两个FIFO*/
  if( (mkfifo(FIFO1, FILE_MODE) < 0) && (errno != EEXIST))
    err_sys("can't create %s", FIFO1);
  if( (mkfifo(FIFO2, FILE_MODE) < 0) &&( errno != EEXIST)) {
    unlink(FIFO1);
    err_sys("can't create %s", FIFO2);
  }
  readfd = open(FIFO1, O_RDONLY, 0);
  writefd = open(FIFO2, O_WRONLY, 0);
  server(readfd, writefd);

  exit(0);
}

