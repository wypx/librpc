/**
 *project:从系统上删除一个共享内寻区
 *author:Xigang WangN
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  int id;
  
  if(argc != 2)
    err_quit("usage:shmrmid <pathname>");
  
  id = Shmget(Ftok(argv[1], 0), 0, SVSHM_MODE);
  Shmctl(id, IPC_RMID, NULL);

  return 0;
}

