/**
 *project:使用popen函数实现的客户-服务器程序
 *author:Xigang Wang
 *email:wangxigang2014@gmail.com
 */
#include "unpipc.h"

int main(int argc, char *argv[])
{
  size_t n;
  char buff[MAXLINE], command[MAXLINE];
  FILE *fp;
  
  /*read 路径名称*/
  Fgets(buff, MAXLINE, stdin);
  n = strlen(buff); 
  if(buff[n-1] == '\n')
    n--; /*删除换行符*/
  
  snprintf(command,sizeof(command), "cat %s", buff);
  /*创建一个通道并启动另一个进程*/
  fp = Popen(command, "r");

  while(Fgets(buff, MAXLINE, fp) != NULL)
    Fputs(buff, stdout);
  
  Pclose(fp);
  exit(0);
}
