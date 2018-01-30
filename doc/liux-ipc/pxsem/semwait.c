/**
 *project:打开一个有名的信号量，调用sem——wait
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  sem_t *sem;
  int val;
  
  if(argc != 2)
    err_quit("usage: semwait <name>");
  
  sem = Sem_open(argv[1], 0);/*打开一个有名的信号量*/
  Sem_wait(sem);/*测试sem是否大于0，是则减1，执行，否者阻塞*/
  Sem_getvalue(sem, &val);/*获取信号的当前值*/
  printf("pid %ld has semaphora value = %d\n", (long) getpid(), val);
  
  pause();
  return 0;
}

