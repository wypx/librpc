/**
 *project:基于内存的信号量的多个生产者-单个消费者程序
 *author：Xigang Wang
 *email：wangixgang2014@gmail.com
 */

#include "unpipc.h"

#define NBUFF 10
#define MAXNTHREADS 100

int nitems, nproducers;

struct{
  int buff[NBUFF];
  int nput;
  int nputval;
  sem_t mutex, nempty, nstored;
}shared;

void *produce(void *arg);
void *consume(void *arg);


int main(int argc, char *argv[])
{
  int i, count[MAXNTHREADS];
  pthread_t tid_produce[MAXNTHREADS], tid_consume;
  
  if(argc != 3)
    err_quit("usage:prodcons3 <#items> <#producers>");
  nitems = atoi(argv[1]);
  nproducers = min(atoi(argv[2]), MAXNTHREADS);
  
  Sem_init(&shared.mutex, 0, 1);
  Sem_init(&shared.nempty, 0, NBUFF);
  Sem_init(&shared.nstored, 0, 0);

  Set_concurrency(nproducers + 1);
  for(i = 0; i < nproducers; ++i){
    count[i] = 0;
    Pthread_create(&tid_produce[i], NULL, produce, &count[i]);
  }
  Pthread_create(&tid_consume, NULL, consume, NULL);
  
  for(i = 0; i < nproducers; ++i){
    Pthread_join(tid_produce[i], NULL);
    printf("count[%d] = %d\n", i, count[i]);
  }
  Pthread_join(tid_consume, NULL);
  
  Sem_destroy(&shared.mutex);
  Sem_destroy(&shared.nempty);
  Sem_destroy(&shared.nstored);
  exit(0);
}


void *produce(void *arg)
{
  for( ; ; ){
    Sem_wait(&shared.nempty);
    Sem_wait(&shared.mutex);
    
    if(shared.nput >= nitems){
      Sem_post(&shared.nempty);
      Sem_post(&shared.mutex);
      return(NULL);
    }
    
    shared.buff[shared.nput % NBUFF] == shared.nputval;
    shared.nput++;
    shared.nputval++;
    
    Sem_post(&shared.mutex);
    Sem_post(&shared.nstored);
    *((int*)arg) += 1;
  }
}

void *consume(void *arg)
{
  int i;
  
  for(i = 0; i < nitems; ++i){
    Sem_wait(&shared.nstored);
    Sem_wait(&shared.mutex);
    
    if(shared.buff[i % NBUFF] != i)
      printf("error: buff[%d] = %d\n", i, shared.buff[i % NBUFF]);

    Sem_post(&shared.mutex);
    Sem_post(&shared.nempty);
  }
  return(NULL);
}
