/**
 *project:system V信号量集的创建，集合中信号量的个数由最后一个参数指定
 *author：Xigang Wang
 *email：wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  int c, oflag, semid, nsems;
  while( (c = Getopt(argc, argv, "e")) != -1){
    switch(c){
    case 'e':
      oflag |= IPC_EXCL;
      break;
    }
  }
  if(optind != argc -2)
    err_quit("usage: semcreate [-e] <pathname> <nsems>");
  nsems = atoi(argv[optind + 1]);
  
  semid = Semget(Ftok(argv[optind], 0), nsems, oflag);/*创建or打开system V信号集*/
  exit(0);
}
