/**
 *project:简单的信号通知（不正确版本）
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 *desc:当有一个信号放置到某个空的队列中，该程序产生一个信号
 */
#include "unpipc.h"

mqd_t mqd;
void *buff;
struct mq_attr attr;
struct sigevent sigev;

static void sig_usr1(int signo);

int main(int argc, char *argv[])
{
  if(argc != 2)
    err_quit("usage: mqnotifysig1 <name>");

  /*打开队列，取得属性，分配读缓冲区*/
  mqd = Mq_open(argv[1], O_RDONLY);
  Mq_getattr(mqd, &attr);
  buff = Malloc(attr.mq_msgsize);

  /*建立信号处理程序，启动通知*/
  Signal(SIGUSR1, sig_usr1);
  sigev.sigev_notify = SIGEV_SIGNAL;
  sigev.sigev_signo = SIGUSR1;
  Mq_notify(mqd, &sigev);

  /*无线循环*/
  for( ; ; )
    pause();
  
  exit(0);
}

/*捕获信号，读出信息*/
static void sig_usr1(int signo)
{
  ssize_t n;
  Mq_notify(mqd, &sigev);
  n = Mq_receive(mqd, buff, attr.mq_msgsize, NULL);
  printf("SIGUSER1 received, read %ld bytes\n", (long)n);
  return;
}
