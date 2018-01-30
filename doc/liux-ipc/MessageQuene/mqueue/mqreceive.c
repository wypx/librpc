/**
 *project:从posix消息队列中读取数据
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */
#include "unpipc.h"

int main(int argc, char *argv[])
{
  int c, flags;
  mqd_t mqd;
  ssize_t n;
  unsigned int prio;
  void *buff;
  struct mq_attr attr;
  
  flags = O_RDONLY;
  while( (c = Getopt(argc, argv, "n")) != -1){/*解析命令行参数*/
    switch(c){
    case 'n':
      flags |= O_NONBLOCK;
      break;
    }
  }
  if(optind != argc -1)
    err_quit("usae: mqreceive [-n] <name>");
  
  mqd = Mq_open(argv[optind], flags);/*创建posix的消息队列*/
  Mq_getattr(mqd, &attr);/*获取消息队列的的属性*/

  buff = Malloc(attr.mq_msgsize);/*分配缓冲区大小，此缓冲区的大小用于mq_receive*/
  
  n = Mq_receive(mqd, buff, attr.mq_msgsize, &prio);/*从消息队列中获取信息*/
  printf("read %ld bytes, priority = %u\n", (long)n,prio);

  exit(0);
}

