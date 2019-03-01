/**
 *project:利用有名信号量实现的生产者-消费者问题
 *author：Xigang Wang
 *email；wangxigang2014@gmail.com
 */

#include "unpipc.h"

/*设置全局变量*/
#define NBUFF 10
#define SEM_MUTEX "mutex"
#define SEM_NEMPTY "nempty"
#define SEM_NSTORED "nstored"

int nitems;
struct{
  int buff[NBUFF];
  sem_t *mutex, *nempty, *nstored;
}shared;

void *produce(void *);
void *consume(void *);

int main(int argc, char *argv[])
{
  pthread_t tid_produce, tid_consume;
  
  if(argc != 2)
    err_quit("usage: prodcons1 <#items>");
  nitems = atoi(argv[1]);
  
  /*创建信号量*/
  shared.mutex = Sem_open(Px_ipc_name(SEM_MUTEX), O_CREAT | O_EXCL, FILE_MODE, 1);
  shared.nempty = Sem_open(Px_ipc_name(SEM_NEMPTY), O_CREAT | O_EXCL, FILE_MODE, NBUFF);
  shared.nstored = Sem_open(Px_ipc_name(SEM_NSTORED), O_CREAT | O_EXCL, FILE_MODE, 0);

  Set_concurrency(2);/*线程并发处理*/
  /*创建两个线程*/
  Pthread_create(&tid_produce, NULL, produce, NULL);
  Pthread_create(&tid_consume, NULL, consume, NULL);
  
  /*主线程等待两个线程*/
  Pthread_join(tid_produce, NULL);
  Pthread_join(tid_consume, NULL);

  /*释放信号量*/
  Sem_unlink(Px_ipc_name(SEM_MUTEX));
  Sem_unlink(Px_ipc_name(SEM_NEMPTY));
  Sem_unlink(Px_ipc_name(SEM_NSTORED));
  return 0;
}

/*生产者*/
void *produce(void *arg)
{
  int i;
  
  for(i = 0; i < nitems; ++i){
    Sem_wait(shared.mutex);/*测试mutex大于0？mutex--：阻塞  这是一个原子操作*/
    Sem_wait(shared.nempty);/*测试nempty大于0？nempty--：阻塞*/
    shared.buff[i % NBUFF] = i;/*向数组中放i*/
    Sem_post(shared.mutex);/*测试mutex等于0？mutex++：阻塞*/
    Sem_post(shared.nstored);
  }
  return(NULL);
}

void *consume(void *arg)
{
  int i;
  
  for(i = 0; i < nitems; ++i){
    Sem_wait(shared.nstored);
    Sem_wait(shared.mutex);
    if(shared.buff[i % NBUFF] != i)
      printf ("buff[%d] = %d\n",i, shared.buff[i % NBUFF]);
    Sem_post(shared.mutex);
    Sem_post(shared.nempty);
  }
  return(NULL);
}
