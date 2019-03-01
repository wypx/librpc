/**
 *project:获取消息队列的资源
 *author:Xigang Wang
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  mqd_t mqd;
  struct mq_attr attr; 
  
  if(argc != 2)
    err_quit("usage: mqgetattr <name>");
  
  /*创建一个Posix的消息队列*/
  mqd = Mq_open(argv[1], O_RDONLY);
  
  /*获取消息队列的属性*/
  Mq_getattr(mqd, &attr);

  /*输出消息的队列属性的值*/
  printf("max #msgs = %ld, max #bytes/msg = %ld, "
	 "#currently on queue = %ld\n",
	 attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);

  /*关闭消息队列的描述符*/
  Mq_close(mqd);
  exit(0);
}
