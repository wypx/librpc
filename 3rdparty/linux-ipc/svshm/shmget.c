/**
 *project:创建一个指定大小的System V共享共享内存区
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
 
  int oflags =  SVSHM_MODE | IPC_CREAT | IPC_EXCL;
  /*创建由用户指定名称和大小的共享内存区*/
  int id = shmget((key_t)1234, sizeof(struct shared_use_st),  oflags);
  char *ptr = shmat(id, NULL, 0);/*把内存区附接到进程的地址空间*/

  //read
  struct shmid_ds buff;
  shmctl(id, IPC_STAT, &buff);
  for(int i = 0; i < buff.shm_segsz; ++i)
    xxxxx;

  shmctl(id, IPC_RMID, NULL);


  return 0；
}

