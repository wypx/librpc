/**
 *project:创建一个有名信号量， 允许的命令行选项有指定独占创建-e和指定的初值
 *author：Xigang Wang
 *email:wangxigang2014@gmail.com
 */
#include "unpipc.h"

int main(int argc, char *argv[])
{
  int c, flags;
  sem_t *sem;
  unsigned int value;

  flags = O_RDWR | O_CREAT;
  value = 1;/*默认值是1*/
  
  while( (c = Getopt(argc, argv, "ei:")) != -1){
    switch(c){
    case 'e':
      flags |= O_EXCL;
      break;
    case 'i':
      value = atoi(optarg);
      break;
    }
  }
  if(optind != argc -1)
    err_quit("usage: semcreate [-e] [- i initvalue] <name>");
  
  sem = Sem_open(argv[optind], flags, FILE_MODE, value);/*创建or打开一个信号量*/
  
  Sem_close(sem);/*关闭信号量*/
  exit(0);
}
