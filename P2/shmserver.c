#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdio.h>
#include <sys/shm.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

#define KEY "/tmp/11042_11823_p2.cfg"
#define SHMSIZE 1024*1024

struct message{
  int type;
  int pid;  // client pid
  int slno; // increment for every message
  int a;  // any number
  int b;  // any number
  int total;  // total
};

typedef struct message *msgPtr;

int semid, shmid;
int slno = 1;

char *shaddr;

msgPtr findMessage(char *addr);
void printRemainingMessages();

void sigHandler(int signo){
  printRemainingMessages();
  shmctl(shmid, IPC_RMID, 0);
  semctl(semid, IPC_RMID, 0);
  exit(0);
}

void printWriteMessage(int slno, int a, int b){
  printf("PID: %d(server), slno: %d, a: %d, b: %d, sum: %d, shmid %d\n", getpid(), slno-1, a, b, a+b, shmid);
  int semval;
  semval = semctl(semid, 0, GETVAL);
  printf("SHM Access semaphore: %d, ", semval);
  semval = semctl(semid, 1, GETVAL);
  printf("Client write semaphore: %d, ", semval);
  semval = semctl(semid, 2, GETVAL);
  printf("Server read semaphore: %d, ", semval);
  semval = semctl(semid, 3, GETVAL);
  printf("Client read semaphore: %d\n", semval);
  fflush(stdout);
}

void printPIDMessage(int slno, int pid, int a, int b){
  printf("PID: %d(client), slno: %d, a: %d, b: %d, sum: %d, shmid %d\n",pid , slno-1, a, b, a+b, shmid);
}

void printRemainingMessages()
{
  msgPtr msg;
  while((msg = findMessage(shaddr)) != NULL)
  {
    printPIDMessage(++slno, msg->pid, msg->a, msg->b);
    msg->type = 0;
  }
}


int main(){
  signal(SIGKILL, sigHandler);
  signal(SIGINT, sigHandler);
  signal(SIGQUIT, sigHandler);
  fopen(KEY, "w+");
  int key = ftok(KEY, 'A');
  msgPtr msg;
  // 4 semaphores to be created-
  // 1. Shm access
  // 2. Client write
  // 3. Server read
  // 4. Client read
  semid = semget(key, 4, IPC_CREAT | 0666);
  struct sembuf sb[4];
  if(semid < 0){
    perror("Semaphore error");
    exit(1);
  }
  // Create shared memory
  shmid = shmget(key, SHMSIZE, IPC_CREAT | IPC_EXCL | 0666);
  if(shmid == -1){
    perror("Shared memory creation error");
    exit(1);
  }
  // Attach to shared memory
  shaddr = shmat(shmid, 0, SHM_RND);
  if(shaddr == (void *) -1){
    perror("Shared memory attachment error");
    exit(1);
  }
  for(;;)
  {
    // Wait on server read and shm access
    sb[1].sem_num = 2;
    sb[1].sem_op = -1;
    sb[1].sem_flg = 0;
    sb[0].sem_num = 0;
    sb[0].sem_op = 0;
    sb[0].sem_flg = 0;
    sb[2].sem_num = 0;
    sb[2].sem_op = 1;
    sb[2].sem_flg = 0;
    if(semop(semid, sb, 3) == -1)
    {
      printf("Semaphore op failed\n");
      exit(1);
    }
    // Server reads message and writes back
    msg = findMessage(shaddr);
    if(msg == NULL){
      // No message for server.
      // Release shm access and set client read
      sb[0].sem_num = 0;
      sb[0].sem_op = -1;
      sb[0].sem_flg = 0;
      // If client write is set, only then is there client waiting to read
      int clientWrite = semctl(semid, 1, GETVAL);
      if(clientWrite == 1)
      {
        sb[1].sem_num = 3;
        sb[1].sem_op = 1;
        sb[1].sem_flg = 0;
      }
      else
      {
        // No client is waiting. Server just got read access
        sb[1].sem_num = 2;
        sb[1].sem_op = 1;
        sb[1].sem_flg = 0;
      }
      if(semop(semid, sb, 2) == -1)
      {
        printf("Semaphore op failed\n");
        exit(1);
      }
    }
    else{
      msgPtr servMsg = malloc(sizeof(struct message));
      servMsg->type = msg->pid;
      servMsg->pid = msg->pid;
      servMsg->slno = slno++;
      servMsg->a = msg->a;
      servMsg->b = msg->b;
      servMsg->total = msg->a + msg->b;
      // Copy the message into the shared memory
      memcpy(msg, servMsg, sizeof(struct message));
      // Set the semaphores of shm access and client read
      printWriteMessage(slno, msg->a, msg->b);
      sb[0].sem_num = 0;
      sb[0].sem_op = -1;
      sb[0].sem_flg = 0;
      // If client write is set, only then is there client waiting to read
      int clientWrite = semctl(semid, 1, GETVAL);
      if(clientWrite == 1)
      {
        sb[1].sem_num = 3;
        sb[1].sem_op = 1;
        sb[1].sem_flg = 0;
      }
      else
      {
        // No client is waiting. Server just got read access
        sb[1].sem_num = 2;
        sb[1].sem_op = 1;
        sb[1].sem_flg = 0;
      }
      if(semop(semid, sb, 2) == -1)
      {
        printf("Semaphore op failed\n");
        exit(1);
      }
    }
  }

  // Put this all in a while loop.
  shmctl(shmid, IPC_RMID, 0);
  semctl(semid, IPC_RMID, 0);
  return 0;
}

msgPtr findMessage(char *addr){
  // Find the first message of type 1 in the shared memory
  // Parameter passed will always be address to first block in shared memory
  msgPtr msg;
  msg = (msgPtr) addr;
  void *endPtr = addr + SHMSIZE*sizeof(char);
  while((void *)msg < endPtr && msg->type != 1){
    // Increment by size of a message
    msg += sizeof(struct message);
  }
  if ((void *)msg > endPtr)
    return NULL;

  return msg;
}
