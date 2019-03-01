/**
 *project:创建一个随内核的持续性的Posix的消息队列
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */
#include "unpipc.h"

int main(int argc, char *argv[])
{
  int c, flags;
  mqd_t mqd;
  
  flags = O_RDWR | O_CREAT;
  while( (c = Getopt(argc, argv, "e")) != -1){/*Getopt解析命令行参数*/
    switch(c){
    case 'e':
      flags |= O_EXCL;
      break;
    }
  }

  if(optind != argc -1)
    err_quit("usage: mqcreate [-e] <pathname>");

  mqd = Mq_open(argv[optind], flags, FILE_MODE, NULL);/*创建一个Posix的消息队列*/

  Mq_close(mqd);/*关闭消息队列返回的描述符，但并没有释放资源，资源还在内核中*/
  exit(0);
}
