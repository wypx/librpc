#include "unpipc.h"

void server(int readfd, int writefd)
{
  int fd;
  ssize_t n;
  char buff[MAXLINE+1];
  
  /*从管道读路径名*/
  if( (n = Read(readfd, buff, MAXLINE)) == 0)
    err_quit("end-of -file while reading pathname");
  buff[n] = '\0';
  
  if( (fd = open(buff, O_RDONLY)) < 0){/*打开文件处理错误*/
    snprintf(buff+n, sizeof(buff) - n, ":can't open, %s\n", 
	     strerror(errno));
    n = strlen(buff);
    Write(writefd, buff, n);
  }else{
    while( (n = Read(fd, buff, MAXLINE)) > 0)/*把文件复制到管道*/
      Write(writefd, buff, n);
    Close(fd);
  }
}
