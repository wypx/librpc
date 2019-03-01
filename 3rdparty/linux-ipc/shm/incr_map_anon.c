/**
 *project:4.4BSD匿名内存映射
 *author:Xigang Wang
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

#define SEM_NAME "mysem"

int main(int argc, char *argv[])
{
  int i, nloop;
  int *ptr;
  sem_t *mutex;
  
  if(argc != 2)
    err_quit("usage: incr_map_anon <#loop>");
  nloop = atoi(argv[1]);
  
  ptr = Mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
	     MAP_SHARED, -1, 0);
  
  mutex = Sem_open(SEM_NAME, O_CREAT | O_EXCL, FILE_MODE, 1);
  Sem_unlink(SEM_NAME);
  
  setbuf(stdout, NULL);/*把标准输出设置为非缓冲区*/
  if(Fork() == 0){
    for(i = 0; i < nloop; ++i){
      Sem_wait(mutex);
      printf ("child:%d\n",(*ptr)++);
      Sem_post(mutex);
    }
    exit(0);
  }
  
  for(i = 0; i < nloop; ++i){
    Sem_wait(mutex);
    printf ("parent:%d\n",(*ptr)++);
    Sem_post(mutex);
  }
  return 0;
}

