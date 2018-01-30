/**
 *project:mqsend发送数据到posix消息队列
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  mqd_t mqd;
  void *ptr;
  size_t len;
  unsigned int prio;

  if(argc != 4)
    err_quit("usage: mqsend <name> <#bytes> <priority>");
  len = atoi(argv[2]);
  prio = atoi(argv[3]);
  
  mqd = Mq_open(argv[1], O_WRONLY);/*创建posix的消息队列*/
  
  ptr = Calloc(len, sizeof(char));/*分配len个sizeof（char）大小的缓冲区，初始化为0*/
  Mq_send(mqd, ptr, len, prio);/*向消息队列中发送消息*/
  
  exit(0);
}
