/**
 *project:挂出有名信号量（把它的值加1），然后输出该信号的值
 *aithor：Xigang Wang
 *email：wangixgang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  sem_t *sem;
  int val;
  
  if(argc != 2)
    err_quit("usage: sempost <name>");
  
  sem = Sem_open(argv[1], 0);
  Sem_post(sem);
  Sem_getvalue(sem, &val);
  printf("value = %d\n", val);

  return 0;
}

 
