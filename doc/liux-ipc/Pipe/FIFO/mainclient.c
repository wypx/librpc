/**
 *project:处理多个客户的FIFO服务器程序客户端
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 *desc:FIFO实现的是单信道，利用FIFO的单个服务器，多个客户的客户-服务器程序
 */
#include "fifo.h"

int main(int argc, char *argv[])
{
  int readfifo, writefifo;
  size_t len;
  ssize_t n;
  char *ptr, fifoname[MAXLINE], buff[MAXLINE];
  pid_t pid;
  
  /*创建客户端的FIFO*/
  pid = getpid();
  snprintf(fifoname, sizeof(fifoname), "/tmp/fifo/%ld", (long)pid);
  if( (mkfifo(fifoname, FILE_MODE)) < 0 && (errno != EEXIST))
    err_sys("can't create %s", fifoname);
  
  snprintf(buff, sizeof(buff), "%ld", (long)pid);/*将PID存放到buff中*/
  len = strlen(buff);
  ptr = buff + len;
  
  Fgets(ptr, MAXLINE - len, stdin);/*从标准输入中输入要求的路径名*/
  len = strlen(buff);
  
  writefifo = Open(SERV_FIFO, O_WRONLY, 0);/*打开客户端FIFO的写*/
  Write(writefifo, buff, len);  /*将PID 空格 路径名组合的字符串写到服务端FIFO*/
  
  readfifo = Open(fifoname, O_RDONLY, 0);/*打开客户端FIFO来读*/
  
  while( (n = Read(readfifo, buff, MAXLINE)) > 0)/*从服务器FIFO读文件按的内容*/
    Write(STDOUT_FILENO, buff, n);/*将要求的内容写到标准输出*/
  Close(readfifo);
  Unlink(fifoname);
  exit(0);
}
