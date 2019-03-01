/**
 *project:记录上锁demo
 *author：Xigang Wang
 *email:wangxigang2014@gmail.com
 */

#include "unpipc.h"

/*可以将下面的请求锁和释放锁，用简单的宏来实现，而且不受struct结构的限制*/
#define read_lock(fd, offset, whence, len) \
  lock_reg(fd, F_SETLK, F_RDLCK, offset, whence, len)
#define readw_lock(fd, offset, whence, len) \
  lock_reg(fd, F_SETLKW, F_RDLCK, offset, whence, len)
#define write_lock(fd, offset, whence, len) \
  lock_reg(fd, F_SETLK, F_WRLCK, offset, whence, len)
#define writew_lock(fd, offset, whence, len) \
  lock_reg(fd, F_SETLKW, F_WRLCK, offset, whence, len)
#deifne un_lock(fd, offset, whence, len) \
  lock_reg(fd, F_SETLK, F_UNLCK, offset, whence, len)
#define is_read_lockable(fd, offset, whene, len) \
  !lock_test(fd, F_RDLCK, offset, whence, len)
#define is_write_lockable(fd, offset, whence, len) \
  !lock_test(fd, F_WRLCK, offset, whence, len)
 
#define SEQFILE "seqno"   /*文件名*/

void my_lock(int);
void my_unlock(int);

int main(int argc, char *argv[])
{
  int fd;
  long i, seqno;
  pid_t pid;
  ssize_t n;
  char line[MAXLINE + 1];
  
  pid = getpid();
  fd = Open(SEQFILE, O_RDWR, FILE_MODE);/*以读写模式打开文件*/
  
  for(i = 0; i < 20; ++i){
    my_lock(fd);/*对指定文件进行加锁*/
    
    Lseek(fd, 0L, SEEK_SET);
    n = Read(fd, line, MAXLINE);
    line[n] = '\0';
    
    n = sscanf(line, "%ld\n", &seqno);/*将line中的序列号保存到seqno中*/
    printf("%s: pid = %ld, seq# = %ld\n", argv[0], (long)pid, seqno);
    
    seqno++;/*序列号加1*/
   
    snprintf(line, sizeof(line), "%ld\n", seqno);/*将序列号保存到line缓冲区中*/
    Lseek(fd, 0L, SEEK_SET);
    Write(fd, line, strlen(line));/*将line缓冲区中的内容写到指定的文件中*/

    my_unlock(fd);/*释放锁*/
  }
n  exit(0);
}

/*加锁*/
/**
void my_lock(int fd)
{
  return;
}
**/
/*释放锁*/
/**
void my_unlock(int fd)
{
  return;
}
**/


 /**
  *所写的锁受flock结构的限制，由上面的宏来实现则不受这种结构的限制
  */
void my_lock(int fd)
{
  struct flock lock;
  
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  Fcntl(fd, F_SETLKW, &lock);
}

void my_unlock(int fd)
{
  struct flock lock;
  
  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;
  
  Fcntl(fd, F_SETLK, &lock);
}


int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
  struct flock lock;
  
  lock.l_type = type;
  lock.l_start = offset;
  lock.l_whence = whence;
  lock.l_len = len;

  return( fcntl(fd, cmd, &lock));
}

pid_t lock_test(int fd, int type, off_t offset, int whence, off_t len)
{
  struct flock lock;
  
  lock.l_type = type;
  lock.l_start = offset;
  lock.l_whence = whence;
  lock.l_len = len;
  
  if(fcntl(fd, F_GETLK, &lock) == -1)
    return(-1);
  
  if(lock.l_type == F_UNLCK)
    return(0);
  return(lock.l_pid);
}
