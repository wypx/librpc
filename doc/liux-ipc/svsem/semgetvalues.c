/**
 *project:取得systemV信号量集中的值
 *author：Xigang Wang
 *email:wangxigang2104@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  int semid, nsems, i;
  struct semid_ds seminfo;/*系统为每一个信号量集维护一个这样的结构*/
  unsigned short *ptr;
  union semun arg;
  
  if(argc != 2)
    err_quit("usage: semgetvalues <pathname> ");
  
  semid = Semget(Ftok(argv[1], 0), 0, 0);/*打开信号量集*/
  arg.buf = &seminfo;
  Semctl(semid, 0, IPC_STAT, arg);/*返回一个semid_ds的结构*/
  nsems = arg.buf->sem_nsems;
  
  ptr = Calloc(nsems, sizeof(unsigned short));/*分配nsems个sizeof（unsigned short）大小的空间*/
  arg.array = ptr;/*union结构中的array指向ptr*/
 
  Semctl(semid, 0, GETALL, arg);/*获得信号量集中的所有值*/
  for(i = 0; i < nsems; ++i)
    printf ("semval[%d] = %d\n", i, ptr[i]);/*打印出来*/
  
  exit(0);
}

