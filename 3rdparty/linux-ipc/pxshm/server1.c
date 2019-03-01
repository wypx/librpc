/**
 *project:给一个共享的计数器持续加1
 *author：Xigang Wang
 *email:wangxigang2014@gmail.com
 *desc:创建并初始化共享内存区和信号量的程序
 */

#include "unpipc.h"

struct shmstruct{
  int count;/*计数器*/
};
sem_t *mutex;/*指向信号量的指针*/

int main(int argc, char *argv[])
{
  int fd;
  struct shmstruct *ptr;
  
  if(argc != 3)
    err_quit("usage:server1 <shmname> <semname>");
  
  shm_unlink(Px_ipc_name(argv[1]));/*防止共享内存区对象已存在*/
  
  fd = Shm_open(Px_ipc_name(argv[1]), O_RDWR | O_CREAT | O_EXCL, FILE_MODE);
  Ftruncate(fd, sizeof(struct shmstruct));
  ptr = Mmap(NULL, sizeof(struct shmstruct), PROT_READ | PROT_WRITE,
	     MAP_SHARED, fd, 0);
  Close(fd);
  
  sem_unlink(Px_ipc_name(argv[2]));
  mutex = sem_open(Px_ipc_name(argv[2]), O_CREAT | O_EXCL, FILE_MODE, 1);
  Sem_close(mutex);
  
  exit(0);
}
