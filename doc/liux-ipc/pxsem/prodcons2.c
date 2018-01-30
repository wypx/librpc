/**
 *project:基于内存信号量的一对一生产者-消费者程序
 *author：Xigang Wang
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

#define NBUFF 10

int nitems;
struct{
  int buff[NBUFF];
  sem_t mutex, nempty, nstored;
}shared;

void *produce(void *);
void *consume(void *);

int main(int argc, char *argv[])
{
  pthread_t tid_produce, tid_consume;

  if(argc != 2)
    err_quit("usage: prodcons2 <#items>");
  nitems = atoi(argv[1]);
  
  /*初始化信号量*/
  Sem_init(&shared.mutex, 0, 1);/*0代表进程内线程共享，非0则表示进程间共享，1是初始化初值*/
  Sem_init(&shared.nempty, 0, NBUFF);
  Sem_init(&shared.nstored, 0, 0);
  
  Set_concurrency(2);/*线程间并发*/
  /*创建线程*/
  Pthread_create(&tid_produce, NULL, produce, NULL);
  Pthread_create(&tid_consume, NULL, consume, NULL);
  
  /*主线程等待两个线程*/
  Pthread_join(tid_produce, NULL);
  Pthread_join(tid_consume, NULL);
  
  /*释放信号量*/
  Sem_destroy(&shared.mutex);
  Sem_destroy(&shared.nempty);
  Sem_destroy(&shared.nstored);
  
  return 0;
}

void *produce(void *arg)
{
  int i;
  
  for(i = 0; i < nitems; ++i){
    Sem_wait(&shared.nempty);
    Sem_wait(&shared.mutex);
    shared.buff[i % NBUFF] = i;
    Sem_post(&shared.mutex);
    Sem_post(&shared.nstored);
  }
  return(NULL);
}
void *consume(void *arg)
{
  int i;
  
  for(i = 0; i < nitems; ++i){
    Sem_wait(&shared.nstored);
    Sem_wait(&shared.mutex);
    if(shared.buff[i % NBUFF] != i)
      printf("buff[%d] =  %d\n", i, shared.buff[i % NBUFF]);
    Sem_post(&shared.mutex);
    Sem_post(&shared.nempty);
  }
  return(NULL);
}
