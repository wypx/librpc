/**
 *project:取得并输出某个信号量集中的所有值
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */
#include "unpipc.h"

int main(int argc, char *argv[])
{
  int semid, nsems, i;
  struct semid_ds seminfo;/*系统为每一个信号量集维护一个这样的结构*/
  unsigned short *ptr;
  union semun arg;/*为了可移植性每个union应该由应用程序定义*/
  
  if(argc < 2)
    err_quit("usage: semsetvalues <pathname> [values...]");
  
  semid = Semget(Ftok(argv[1], 0), 0, 0);/*打开信号量集*/
  arg.buf = &seminfo;/*arg.buf参会指向seminfo结构*/
  Semctl(semid, 0, IPC_STAT, arg);
  nsems = arg.buf->sem_nsems;/*获取信号量的数量*/
  
  if(argc != nsems + 2)
    err_quit("%d semaphores in set, %d values specified, nsems, argc + 2");
  
  ptr = Calloc(nsems, sizeof(unsigned short));/*分配nsems个sizeof（unsigned short）大小的空间*/
  arg.array = ptr;/*union结构中的arg.array参数执行刚刚分配的ptr*/
  for(i = 0; i < nsems; ++i)
    ptr[i] = atoi(argv[i + 2]);
  Semctl(semid, 0, SETALL, arg);/*设置信号量的值*/

  exit(0);
}
