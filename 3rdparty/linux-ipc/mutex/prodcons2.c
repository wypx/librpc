/**
 *project:生产者-消费者同步demo2
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */
#include "unpipc.h"

#define MAXNITEMS 1000000
#define MAXNTHREADS 100

/*线程间共享的全局变量*/
int nitems;
struct {
  pthread_mutex_t mutex;
  int buff[MAXNITEMS];
  int nput;/*buff[]下一次存放的元素下标*/
  int nval;/*下一次存放的值*/
}shared = {
  PTHREAD_MUTEX_INITIALIZER/*互斥锁静态初始化*/
};

void *produce(void *);
void *consume(void *);
void consume_wait(int i);

int main(int argc, char *argv[])
{
  int i, nthreads, count[MAXNTHREADS];
  pthread_t tid_produce[MAXNTHREADS], tid_consume;
  
  if(argc != 3)
    err_quit("usage: prodcons2 <#items> <#threads>");
  nitems = min(atoi(argv[1]), MAXNITEMS);
  
  Set_concurrency(nthreads + 1);/*并发设置*/

	       /*创建线程生产者*/
  for(i = 0; i < nthreads; ++i){
    count[i] = 0;
    Pthread_create(&tid_produce[i], NULL, produce, &count[i]);
  }
	       /*创建消费者线程*/
  Pthread_create(&tid_consume, NULL, consume, NULL);
	       /*主线程等待子线程结束*/
  for(i = 0; i < nthreads; ++i){
  Pthread_join(tid_produce[i], NULL);
  printf ("count[%d] = %d\n",i, count[i]);
  }
  Pthread_join(tid_consume, NULL);

  return 0;
}

/*生产者*/
void *produce(void *arg)
{
  for( ; ; ){
    Pthread_mutex_lock(&shared.mutex);
    if(shared.nput >= nitems){
      Pthread_mutex_unlock(&shared.mutex);
      return(NULL);
    }
    shared.buff[shared.nput] = shared.nval;
    shared.nput++;
    shared.nval++;
    Pthread_mutex_unlock(&shared.mutex);
    *((int *)arg) += 1;
  }
}

  /*等待生产者*/
void consume_wait(int i)
{
  for( ; ; ){
    Pthread_mutex_lock(&shared.mutex);
    if(i < shared.nput){
      Pthread_mutex_unlock(&shared.mutex);
      return;
    }
    Pthread_mutex_unlock(&shared.mutex);
  }
}

void *consume(void *arg)
{
  int i;
  
  for(i = 0; i < nitems; ++i){
    consume_wait(i);/*消息等待*/
    if(shared.buff[i] != i)
      printf ("buff[%d] = %d\n",i, shared.buff[i]);
  }
  return(NULL);
}
