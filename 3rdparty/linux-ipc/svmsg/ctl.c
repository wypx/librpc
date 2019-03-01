/**
 *project:创建一个简单的system V 消息队列
 *author：Xigang Wang
 *email:wangxigang2014@gmail.com
 */
#include "unpipc.h"

int main(int argc, char *argv[])
{
  struct msqid_ds info;
  struct msgbuf buf;
  
  int msqid = msgget((key_t)1234, SVMSG_MODE | IPC_CREAT);
  if (msgid == -1) return -1;

  buf.mtype = 1;
  buf.mtext[0] = 1;
  msgsnd(msqid, &buf, 1, 0);

  msgctl(msqid, IPC_STAT, &info);
  printf ("read-write:%03o, cbytes = %lu, qnum = %lu, qbytes = %lu\n",
	  info.msg_perm.mode & 0777, (ulong_t)info.msg_cbytes,
	  (ulong_t)info.msg_qnum, (ulong_t)info.msg_qbytes);

  system("ipcs -q");

  long int msgtype = 0;   // 注意1
  if (msgrcv(msgid, (void *)&buf, BUFSIZ, msgtype, 0) == -1)
    return -1;
  //删除消息队列
  msgctl(msqid, IPC_RMID, NULL);
  exit(0);
}
