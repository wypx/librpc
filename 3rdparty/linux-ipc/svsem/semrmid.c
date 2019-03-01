/**
 *project:从系统中删除一个信号量集
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  int semid;
  
  if(argc != 2)
    err_quit("usage: semrmid <pathname>");
  
  semid = Semget(Ftok(argv[1], 0), 0, 0);/*打开信号量集*/
  Semctl(semid, 0, IPC_RMID);/*在系统中删除信号量集*/

  exit(0);
}

