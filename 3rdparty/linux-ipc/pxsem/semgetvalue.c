/**
 *project:打开一个信号量，取得它的当前值，然后输出该值
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  sem_t *sem;
  int val;
  
  if(argc != 2)
    err_quit("usage: semgetvalue <name>");
  
  sem = Sem_open(argv[1], 0);/*打开信号量*/
  Sem_getvalue(sem, &val);/*取得当前值*/
  printf("value = %d\n", val);

  return 0;
}
