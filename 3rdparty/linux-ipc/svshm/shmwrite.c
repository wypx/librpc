/**
 *project:打开一个共享内存区，向其中填入一个数据模式
 *author：Xigang Wang
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  int i, id;
  struct shmid_ds buff;
  unsigned char *ptr;
  
  if(argc != 2)
    err_quit("usage: shmwrite <pathname>");
  
  id = Shmget(Ftok(argv[1], 0), 0, SVSHM_MODE);
  ptr = Shmat(id, NULL, 0);
  Shmctl(id, IPC_STAT, &buff);
  
  for(i = 0; i < buff.shm_segsz; ++i)
    *ptr++ = i % 256;
  
  return 0;
}
