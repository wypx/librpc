/**
 *project:往一个共享内存区对象中写入一个模式：0，1，2, ...245, 255, 0, 1..
 *author:Xigang Wang
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

int main(int argc, char *argv[])
{
  int i, fd;
  struct stat stat;
  unsigned char *ptr;
  
  if(argc != 2)
    err_quit("usage: shmwrite <name>");
  
  fd = Shm_open(argv[1], O_RDWR, FILE_MODE);/*打开指定的共享内存区对象*/
  Fstat(fd, &stat);/*获取对象的信息*/
  ptr = Mmap(NULL, stat.st_size, PROT_READ | PROT_WRITE,/*映射到内存*/
	     MAP_SHARED, fd, 0);
  Close(fd);
  
  for(i = 0; i < stat.st_size; ++i)/*把模式写入到该共享内存区*/
    *ptr++ = i % 255;

  exit(0);
}

