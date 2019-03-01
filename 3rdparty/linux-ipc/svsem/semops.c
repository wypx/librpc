/**
 *project:对某个信号量集执行一数组的操作（这是一个原子操作）
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  int c, i, flag, semid, nops;
  struct sembuf *ptr;
  
  /*命令行的选项*/
  flag = 0;
  while( (c = Getopt(argc, argv, "nu")) != -1){
    switch(c){
    case 'n':
      flag |= IPC_NOWAIT;/*指定非阻塞操作*/
      break;
    case 'u':
      flag |= SEM_UNDO;
      break;
    }
  }
  if(argc - optind < 2)
    err_quit("usage: semops [-n] [-u] <pathname> operatio...");
  
  semid = Semget(Ftok(argv[optind], 0), 0, 0);
  optind++;
  nops = argc -optind;
  
  /*分配内存*/
  ptr = Calloc(nops, sizeof(struct sembuf));
  for(i = 0; i < nops; ++i){
    ptr[i].sem_num = i;
    ptr[i].sem_op = atoi(argv[optind + i]);
    ptr[i].sem_flg = flag;
  }
  Semop(semid, ptr, nops);
  
  exit(0);
}
