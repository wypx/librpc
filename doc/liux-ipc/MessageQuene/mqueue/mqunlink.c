/**
 *project：释放消息队列的资源
 *author：Xigang Wang
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  if(argc != 2)
    err_quit("usage: mqunlink <name>");
  
  Mq_unlink(argv[1]);/*释放掉内核占用的资源*/
  exit(0);
}
