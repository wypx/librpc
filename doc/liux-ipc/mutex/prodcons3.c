/**
 *project:生产者-消费者问题（互斥锁与条件变量的结合）
 *author：Xigang Wang
 *email:wangxigang2014@gmail.com
  */
#include "unpipc.h"

#define MAXNITEMS 1000000
#define MAXNTHREADS 100

void *produce(void *);
void *comsume(void *);


/*设置全局变变量*/
int nitems;
int buff[MAXNITEMS];
struct {/*把生产者变量和互斥锁收集到一个结构中*/
  pthread_mutex_t mutex;
  int nput;
  int nval;
}put = {
  PTHREAD_MUTEX_INITIALIZER
};

struct {/*把计数器、条件变量、和互斥锁收集到一个结构中*/
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int nready;
}nready = {
  PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER
};

int main(int argc, char *argv[])
{
  int i, nthreads, count[MAXNTHREADS];
  pthread_t tid_produce[MAXNTHREADS], tid_consume;
  
  if(argc != 3)
    err_quit("usage: prodcons2 <#items> <#threads>");
  nitems = min(atoi(argv[1]), MAXNITEMS);
  
  Set_concurrency(nthreads + 1);/*线程并发设置*/

	       /*创建线程生产者*/
  for(i = 0; i < nthreads; ++i){
    count[i] = 0;
    Pthread_create(&tid_produce[i], NULL, produce, &count[i]);
  }
	       /*创建消费者线程*/
  Pthread_create(&tid_consume, NULL, comsume, NULL);
	       /*主线程等待子线程结束*/
  for(i = 0; i < nthreads; ++i){
  Pthread_join(tid_produce[i], NULL);
  printf ("count[%d] = %d\n",i, count[i]);
  }
  Pthread_join(tid_consume, NULL);

  return 0;
}


void *produce(void *arg)
{
  for( ; ; ){/*向数组中放下一个条目*/
    Pthread_mutex_lock(&put.mutex);
    if(put.nput >= nitems){  /*如果数目大于指定数目说明buff已满*/
      Pthread_mutex_unlock(&put.mutex);
      return(NULL);
    }
    buff[put.nput] = put.nval;
    put.nput++;
    put.nval++;
    Pthread_mutex_unlock(&put.mutex);

    /*通知消费者*/
    Pthread_mutex_lock(&nready.mutex);
    if(nready.nready == 0)/*如果nready计数器等于0*/
      Pthread_cond_signal(&nready.cond);/*唤醒消费者线程*/
    nready.nready++;
    Pthread_mutex_unlock(&nready.mutex);
    
    *((int*)arg) += 1;
  }
}

void *comsume(void *arg)
{
  int i;
  
  for(i = 0; i < nitems; ++i){
    /*消费者等待变为非0*/
    Pthread_mutex_lock(&nready.mutex);
    while(nready.nready == 0)/*如果计数器=0*/
      Pthread_cond_wait(&nready.cond, &nready.mutex);/*消费者线程等待*/
    nready.nready--;
    Pthread_mutex_unlock(&nready.mutex);

    if(buff[i] != i)
      printf("buff[%d] = %d\n", i, buff[i]);
  }
  return(NULL);
}
