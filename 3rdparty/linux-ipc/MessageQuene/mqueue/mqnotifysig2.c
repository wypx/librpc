#include "unpipc.h"

volatile sig_atomic_t mqflag;/*定义一个全局变量*/
static void sig_usr1(int);

int main(int argc, char *argv[])
{
  mqd_t mqd;
  void *buff;
  ssize_t n;
  sigset_t zeromask, newmask, oldmask;/*定义三个信号集合*/
  struct mq_attr attr;
  struct sigevent sigev;
  
  if(argc != 2)
    err_quit("usage: mqnotifysig2 <name>");
  
  /*打开消息队列*/
  mqd = Mq_open(argv[1], O_RDONLY);
  Mq_getattr(mqd, &attr);
  buff = Malloc(attr.mq_msgsize);

  /*初始化信号集*/
  Sigemptyset(&zeromask);
  Sigemptyset(&newmask);
  Sigemptyset(&oldmask);
  Sigaddset(&newmask, SIGUSR1);/*在newmask集中打开SIGUSR1信号*/

  
  Signal(SIGUSR1, sig_usr1);
  sigev.sigev_notify = SIGEV_SIGNAL;
  sigev.sigev_signo = SIGUSR1;
  Mq_notify(mqd, &sigev);

  for( ; ; ){
    Sigprocmask(SIG_BLOCK, &newmask, &oldmask);/*阻塞SIGUSR1*/
    while(mqflag == 0)
      sigsuspend(&zeromask);/*原子性的调用线程投入睡眠*/
    mqflag = 0;

    /*当mqflag非零时，从新注册，并从指定的队列中读出消息*/
    Mq_notify(mqd, &sigev);
    n = Mq_receive(mqd, buff, attr.mq_msgsize, NULL);
    printf("read %ld bytes\n", (long)n);
    Sigprocmask(SIG_UNBLOCK, &newmask, NULL);/*解除SIGUSR1阻塞*/
  }
  exit(0); 
 }

static void sig_usr1(int signo)
{
  mqflag = 1;
  return;
}
