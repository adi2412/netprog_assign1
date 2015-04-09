#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <stdio.h>
#include "rshmAPI.h"


/**
 * RSHM API will mainly contain four publicly exposed functions
 * 1. rshmget(key, size)
 * 2. rshmat(rshmid, addr)
 * 3. rshmdt(rshmid, addr)
 * 4. rshmctl(rshmid, cmd)- The only command it supports is
 * IPC_RMID
 * 5. rshmChanged(rshmid)
 *
 * We will use a message queue to send messages to TCP server
 * containing the follow information-
 *  long mtype- Specifies the type of the message. This would be
 *  6 for the message from the server- the assumption being each
 *  server talks to only one local process
 *  1 for rshmget
 *  2 for rshmat
 *  3 for rshmdt
 *  4 for rshmctl
 *  5 for rshmChanged

 * int key- Only useful for one command. 0 everytime else
 * int size- Useful for just the one command. 0 everytime else
 * int rshmid- the rshmid of the shared memory
 * void *addr- Pointer to the shared memory. WARNING: Do not even
 * try manipulating this value in any sense whatsoever. This is
 * segmentation fault bomb waiting to blow up. The only place you
 * can use this correctly is the server. NULL it for commands which
 * do not have this.
 * cmd- the only possible value being IPC_RMID. check if this is 
 * the value. Else send an error.
 *
 * Once you send a message, you also write to the fifo accessed by
 * API and TCP server. This is so that you can synchronize I/O
 * waiting in the TCP server(Can't do this for MSG Qs unfortunately)
 *
 *
 * Utility functions
 * prepareMessage(type, key, size, rshmid, addr, cmd)
 * sendMessage(msg)
 * writeToFifo()- just write a single character into the fifo.
 * 
 * 
 */

msg prepareMessage(int type, key_t key, size_t size, int rshmid, void *addr, int cmd)
{
  msg myMsg;
  if(type == 1)
  {
    // Prepare the get message
    myMsg.mtype = 1;
    myMsg.key = key;
    myMsg.size = size;
    myMsg.rshmid = 0;
    myMsg.addr = NULL;
    myMsg.cmd = 0;
  }
  else if(type == 2)
  {
    //  Prepare the attach message
     myMsg.mtype = 2;
     myMsg.key = 0;
     myMsg.size = 0;
     myMsg.rshmid = rshmid;
     myMsg.addr = addr;
     myMsg.cmd = 0;
  }
  else if(type == 3)
  {
    // Prepare the detach message
    myMsg.mtype = 3;
    myMsg.key = 0;
    myMsg.size = 0;
    myMsg.rshmid = rshmid;
    myMsg.addr = addr;
    myMsg.cmd = cmd;
  }
  else if(type == 4)
  {
    // Prepare the ctl message
    myMsg.mtype = 4;
    myMsg.key = 0;
    myMsg.size = 0;
    myMsg.rshmid = rshmid;
    myMsg.addr = NULL;
    myMsg.cmd = IPC_RMID;
  }
  else if(type == 5)
  {
    // Prepare the changed message
    myMsg.mtype = 5;
    myMsg.key = 0;
    myMsg.size = 0;
    myMsg.rshmid = rshmid;
    myMsg.addr = NULL;
    myMsg.cmd = 0;
  }
  else
  {
    // Invalid type
  }
  return myMsg;
}

int getMessageQ()
{
  fopen(KEY, "w+");
  key_t key;
  key = ftok(KEY, '4');
  int qid = msgget(key, IPC_CREAT | 0666);
  if(qid == -1)
  {
    perror("Message queue");
  }
  return qid;
}

int sendMessage(msg message)
{
  int qid, result, length;
  qid = getMessageQ();
  length = sizeof(msg) - sizeof(long);
  if((result = msgsnd(qid, &message, length, 0)) == -1)
  {
    perror("Message queue send");
  }
  return result;
}

msg receiveMessage()
{
  msg rcvMsg;
  int qid, length, result;
  qid = getMessageQ();
  length = sizeof(msg) - sizeof(long);
  printf("Receiving message\n");
  if((result = msgrcv(qid, &rcvMsg, length, 6, 0)) == -1)
  {
    perror("Message queue receive");
  }
  return rcvMsg;
}

int writeToFifo()
{
  int res, fifo;
  char a = '1';
  fifo = open(FIFO, O_WRONLY);
  res = write(fifo, &a, sizeof(char));
  close(fifo);
  if(res == -1)
    perror("fifo");
  return res;
}

// int main(){
//   printf("This is the API. Call it from your source code\n");
// }

int rshmget(key_t key, size_t size)
{
  // Prepare a message with the key and the size
  msg myMsg = prepareMessage(1, key, size, 0, NULL, 0);
  msg replyMsg;
  // Send message to TCP server
  sendMessage(myMsg);
  // Write to fifo that message has been written
  writeToFifo();
  // Wait for reply message to arrive
  replyMsg = receiveMessage();
  // Send the rshmid back to the client
  return replyMsg.resp;
}

void * rshmat(int rshmid, void *addr)
{
  msg myMsg = prepareMessage(2, 0, 0, rshmid, addr, 0);
  msg replyMsg;
  sendMessage(myMsg);
  writeToFifo();
  replyMsg = receiveMessage();
  printf("Attached: %p\n", replyMsg.addr);
  // Attach to shared memory
  void *address;
  address = shmat(replyMsg.shmid, addr, 0);
  if(address == (void *) -1)
    perror("Attachment error");
  printf("%p\n", address);
  return address;
}

int rshmdt(int rshmid, void *addr)
{
  msg myMsg = prepareMessage(3, 0, 0, rshmid, addr, 0);
  msg replyMsg;
  sendMessage(myMsg);
  writeToFifo();
  replyMsg = receiveMessage();
  printf("Response: %d\n", replyMsg.resp);
  // Detach shared memory
  shmdt(addr);
  // Reply with the response?
  return 0;
}

int rshmCtl(int rshmid, int cmd)
{
  // Check the command value
  if(cmd == IPC_RMID)
  {
    msg myMsg = prepareMessage(4, 0, 0, rshmid, NULL, cmd);
    msg replyMsg;
    sendMessage(myMsg);
    writeToFifo();
    replyMsg = receiveMessage();
    printf("Response: %d\n", replyMsg.resp);
    return replyMsg.resp;
  }
  else
  {
    // Invalid command
    return -1;
  }
}

void rshmChanged(int rshmid)
{
  msg myMsg = prepareMessage(5, 0, 0, rshmid, NULL, 0);
  msg replyMsg;
  sendMessage(myMsg);
  writeToFifo();
  replyMsg = receiveMessage();
}