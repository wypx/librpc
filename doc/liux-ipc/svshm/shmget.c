/**
 *project:创建一个指定大小的System V共享共享内存区
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  int c, id, oflags;
  char *ptr;
  size_t length;
  
  oflags = SVSHM_MODE | IPC_CREAT;
  while( (c = Getopt(argc, argv, "e")) != -1){
    switch(c){
    case 'e':
      oflags |= IPC_EXCL;
      break;
    }
  }
  if(optind != argc - 2)
    err_quit("usage: shmget [-e] <pathname> <length>");
  length = atoi(argv[optind + 1]);
  
  id = Shmget(Ftok(argv[optind], 0), length, oflags);/*创建由用户指定名称和大小的共享内存区*/
  ptr = Shmat(id, NULL, 0);/*把内存区附接到进程的地址空间*/

  return 0；
}

