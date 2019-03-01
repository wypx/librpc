/**
 *project:创建posix的消息队列
 *author：Xigang Wang
 *email:wangixgang2014@gmail.com
 *desc:可以设置消息队列的属性
 */

#include "unpipc.h"

struct mq_attr attr;

int main(int argc, char *argv[])
{
  int c, flags;
  mqd_t mqd;
  
  flags = O_RDWR | O_CREAT;
  while( (c = Getopt(argc, argv, "em:z:")) !=-1){/*Getopt命令行参数解析*/
    switch(c){
    case 'e':
      flags |= O_EXCL;
      break;
    case 'm':
      attr.mq_maxmsg = atol(optarg);
      break;
    case 'z':
      attr.mq_msgsize = atol(optarg);
      break;
    }
  }

  if(optind != argc -1)
    err_quit("usage； mqcreate [-e] [-m maxmsg -z msgsize] <name>");

  if((attr.mq_maxmsg != 0 && attr.mq_msgsize == 0) ||
     (attr.mq_maxmsg == 0 && attr.mq_msgsize != 0))	
    err_quit("must specify both -m maxmsg and -z msgszie");

  mqd = Mq_open(argv[optind], flags, FILE_MODE, (attr.mq_maxmsg != 0)? &attr:NULL);
  Mq_close(mqd);
  exit(0);
}
