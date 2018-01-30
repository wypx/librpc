/**
 *project：删除一个有名的信号量
 *author：Xigang 
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  if(argc != 2)
    err_quit("usage: semunlink <name>");

  Sem_unlink(argv[1]);
  
  exit(0);
}

