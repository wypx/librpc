/**
 *project:父子进程给同一个全局变量加1
 *author：Xigang Wang
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

#define SEM_NAME "mysem"

int count = 0;

int main(int argc, char *argv[])
{
  int i, nloop;
  sem_t *mutex;
  
  if(argc != 2)
    err_quit("usage: incr1  <#loops>");
  nloop = atoi(argv[1]);

  /*创建并初始化信号量*/
  mutex = Sem_open(Px_ipc_name(SEM_NAME), O_CREAT | O_EXCL, FILE_MODE, 1);
  Sem_unlink(Px_ipc_name(SEM_NAME));
  /*把标准输出设置为非缓冲区*/
  setbuf(stdout, NULL);
	    
  if(Fork() == 0){/*child*/
    for(i = 0; i < nloop; ++i){
	Sem_wait(mutex);
	printf("child: %d\n", count++);
	Sem_post(mutex);
	}
    exit(0);
   }
	     
  for(i = 0; i < nloop; ++i){/*parent*/
	Sem_wait(mutex);
	printf ("parent: %d\n", count++);
	Sem_post(mutex);
      }	     
  exit(0);
}
































