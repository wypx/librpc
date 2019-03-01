/**
 *project:计数器和信号量都在共享内存中
 *author:Xigang Wang
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

struct shared{/*定义将存放在共享内存区中的结构*/
  sem_t mutex;/*信号量*/
  int count;/*计数器*/
}shared;

int main(int argc, char *argv[])
{
  int fd, i, nloop;
  struct shared *ptr;
  
  if(argc != 3)
    err_quit("usage: incr3 <pathname> <#loop>");
  nloop = atoi(argv[2]);
  
  /*映射到内存*/
  fd = Open(argv[1], O_RDWR | O_CREAT, FILE_MODE);
  Write(fd, &shared, sizeof(struct shared));
  ptr = Mmap(NULL, sizeof(struct shared), PROT_READ | PROT_WRITE, 
	     MAP_SHARED, fd, 0);
  Close(fd);
  
  Sem_init(&ptr->mutex, 1, 1);/*初始化信号量*/
  
  setbuf(stdout, NULL);
  if(Fork() == 0){
    for(i = 0; i < nloop; ++i){
      Sem_wait(&ptr->mutex);
      printf("child:%d\n", ptr->count++);
      Sem_post(&ptr->mutex);
    }
    exit(0);
  }
  for(i = 0; i < nloop; ++i){
    Sem_wait(&ptr->mutex);
    printf("parent: %d\n", ptr->count++);
    Sem_post(&ptr->mutex);
  }
  return 0;
}

