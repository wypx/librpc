/**
 *project:使用sigwait代替信号处理程序的信号通知
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  int signo;
  mqd_t mqd;
  void *buff;
  ssize_t n;
  sigset_t newmask;
  struct mq_attr attr;
  struct sigevent sigev;

  if(argc != 2)
    err_quit("usage: mqnotify3 <name>");
  
  /*创建队列，获取消息队列的属性，给buff分配空间*/
  mqd = Mq_open(argv[1], O_RDONLY | O_NONBLOCK);
  Mq_getattr(mqd, &attr);
  buff = Malloc(attr.mq_msgsize);
  
  Sigemptyset(&newmask);/*初始化信号集*/
  Sigaddset(&newmask, SIGUSR1);/*在信号集中打开SIGUSR1信号*/
  Sigprocmask(SIG_BLOCK, &newmask, NULL);/*SIGUSR1阻塞*/
  
  /*启动通知*/
  sigev.sigev_notify = SIGEV_SIGNAL;
  sigev.sigev_signo = SIGUSR1;
  Mq_notify(mqd, &sigev);

  for( ; ; ){
    Sigwait(&newmask, &signo);/*等待待处理信号，SIGUSR1信号出现保存在signo中*/
    if(signo == SIGUSR1){
      Mq_notify(mqd, &sigev);/*从新注册*/
      while( (n = mq_receive(mqd, buff, attr.mq_msgsize, NULL)) >= 0){
	printf("read %ld bytes\n", (long) n);
      }
      if(errno != EAGAIN)
	err_sys("mq_receive error");
    }
  }
  exit(0);
}

