/**
 *project:父子进程给共享内存区的一个计数器加1
 *author:Xigang Wang
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

#define SEM_NAME "mysem"

int main(int argc, char *argv[])
{
  int fd, i, nloop, zero = 0;
  int *ptr;
  sem_t *mutex;
  
  if(argc != 3)
    err_quit("usage: incr2 <pathname> <#loops>");
  
  fd = Open(argv[1], O_RDWR | O_CREAT, FILE_MODE);/*打开文件用于读写，不存在则创建它*/
  Write(fd, &zero, sizeof(int));/*写一个值为0的值保存到文件*/
  /*调用mmap把刚打开的文件映射到本进程的内存空间中*/
  ptr = Mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  Close(fd);
  
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
