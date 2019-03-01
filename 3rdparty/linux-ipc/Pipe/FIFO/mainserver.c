/**
 *project:处理多个客户的FIFO服务器程序服务端
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 *desc:FIFO实现的是单信道，利用FIFO的单个服务器，多个客户的客户-服务器程序
 */
#include "fifo.h"

void server(int, int);

int main(int argc, char *argv[])
{
  int readfifo, writefifo, dummyfd, fd;
  char *ptr, buff[MAXLINE+1], fifoname[MAXLINE];
  pid_t pid;
  ssize_t n;

  /*创建众所周知FIFO，打开来读，打开来写*/
  if( (mkfifo(SERV_FIFO, FILE_MODE)) < 0 && (errno != EEXIST))
    err_sys("can't create %s", SERV_FIFO);
  
  readfifo = Open(SERV_FIFO, O_RDONLY, 0);
  dummyfd = Open(SERV_FIFO, O_WRONLY, 0);/*从来没有用过*/
  
  while( (n = Readline(readfifo, buff, MAXLINE)) > 0){/*从fifo中读出客户的请求，有PID 空格 和路径名构成*/
    if(buff[n-1] == '\n')  /*删除换行符*/
      n--;
    buff[n] = '\0';
    
    if( (ptr = strchr(buff, ' ')) == NULL){
      err_msg("bpgus request: %S", buff);
      continue;
    }
    *ptr++ = 0;
    pid = atol(buff);   /*将字符串PID转换为数值型PID*/
    snprintf(fifoname, sizeof(fifoname), "/tmp/fifo/%ld", (long)pid);/*获取客户端FIFO的pathname*/
    /*打开客户请求的文件，将它发送到客户的FIFO中*/
    if( (writefifo = open(fifoname, O_WRONLY, 0)) < 0){/*打开连接客户的FIFO，为给客户发送数据做准备*/
      err_msg("cannot open:%s", fifoname);
      continue;
    }
    if( (fd = open(ptr, O_RDONLY)) < 0){/*打开服务器上的客户要求的指定文件*/
      snprintf(buff+n, sizeof(buff)-n, ":can't open, %s\n",   
	       strerror(errno));/*错误处理*/
      n = strlen(ptr);
      Write(writefifo, ptr, n);
      Close(writefifo);
    }else{
      while( (n = Read(fd, buff, MAXLINE)) > 0)
	Write(writefifo, buff, n);/*将要的文件内容发给它*/
      Close(fd);
      Close(writefifo);
    }
  }
  exit(0);
}
