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
#define MAX 255

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

msgPtr findMessage(char *addr);

msgPtr findFreeMemory(char *addr);

void printWriteMessage(int slno, int a, int b){
  printf("PID: %d, slno: %d, a: %d, b: %d, shmid %d\n", getpid(), slno-1, a, b, shmid);
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

int main()
{
  fopen(KEY, "w+");
  int key = ftok(KEY, 'A');
  char *shaddr;
  char input[MAX] = "";
  char num1[MAX], ch;
  char *numbers, *res;
  char num2[MAX];
  int a, b;
  msgPtr msg;
  int slno = 1;
  // 4 semaphores to be created-
  // 1. Shm access
  // 2. Client write
  // 3. Server read
  // 4. Client read
  semid = semget(key, 4, IPC_CREAT | 0666);
  struct sembuf sb[5];
  if(semid < 0){
    perror("Semaphore error");
    exit(1);
  }
  // Create shared memory
  shmid = shmget(key, SHMSIZE, IPC_CREAT | 0666);
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
    // Wait on client write, shm access
    // Set client write and shm access
    printf("Please wait for your turn\n");
    sb[0].sem_num = 1;
    sb[0].sem_op = 0;
    sb[0].sem_flg = 0;
    sb[1].sem_num = 0;
    sb[1].sem_op = 0;
    sb[1].sem_flg = 0;
    sb[2].sem_num = 1;
    sb[2].sem_op = 1;
    sb[2].sem_flg = SEM_UNDO;
    sb[3].sem_num = 0;
    sb[3].sem_op = 1;
    sb[3].sem_flg = SEM_UNDO;
    if(semop(semid, sb, 4) == -1)
    {
      perror("Semaphore op failed\n");
      exit(1);
    }
    int servRead = semctl(semid, 2, GETVAL);
    if(servRead == 1)
    {
      sb[0].sem_num = 2;
      sb[0].sem_op = -1;
      sb[0].sem_flg = 0;
      if(semop(semid, sb, 1) == -1)
      {
        perror("Semaphore op failed\n");
        exit(1);
      }
    }
    printf("Enter two numbers separated by spaces to add. You can keep entering numbers until you press Ctrl-D.\n");
    char input2[MAX];
    while(fgets(input2, sizeof(input2), stdin) != NULL)
    {
      input2[strlen(input2)-1] = '\0';
      numbers = strtok(input2, " ");
      a = atoi(numbers);
      numbers = strtok(NULL, " ");
      b = atoi(numbers);

      // Perpare the message
      msg = findFreeMemory(shaddr);
      if(msg == NULL)
      {
        printf("Shared memory is full. Can't send your message\n");
      }
      else
      {
        // Write your message to server
        msgPtr sndMsg = malloc(sizeof(struct message));
        sndMsg->type = 1;
        sndMsg->pid = getpid();
        sndMsg->slno = slno++;
        sndMsg->a = a;
        sndMsg->b = b;
        sndMsg->total = 0;
        printWriteMessage(slno, a, b);
        // Copy the message into the shared memory
        memcpy(msg, sndMsg, sizeof(struct message));
      }
    }
    // Fix for OSX
    clearerr(stdin);
    // All messages sent by client

    // Set semaphores of shm access and server read
    sb[0].sem_num = 0;
    sb[0].sem_op = -1;
    sb[0].sem_flg = 0;
    sb[1].sem_num = 2;
    sb[1].sem_op = 1;
    sb[1].sem_flg = 0;
    if(semop(semid, sb, 2) == -1)
    {
      perror("Semaphore op failed\n");
      exit(1);
    }
    // Wait on client read and shm access
    sb[1].sem_num = 3;
    sb[1].sem_op = -1;
    sb[1].sem_flg = 0;
    sb[0].sem_num = 0;
    sb[0].sem_op = 0;
    sb[0].sem_flg = 0;
    sb[2].sem_num = 0;
    sb[2].sem_op = 1;
    sb[2].sem_flg = SEM_UNDO;
    if(semop(semid, sb, 3) == -1)
    {
      perror("Semaphore op failed\n");
      exit(1);
    }

    // Read the answer
    msg = findMessage(shaddr);
    if(msg == NULL){
      printf("No message found to read\n");
    }
    else
    {
      printf("Result of %d+%d is: %d \n", msg->a, msg->b, msg->total);
      // Remove the message from the shared memory
      // An easy way is to change the type of the message to 0
      msg->type = 0;
    }

    // Release shm access, set server read and client write
    sb[0].sem_num = 0;
    sb[0].sem_op = -1;
    sb[0].sem_flg = 0;
    sb[1].sem_num = 2;
    sb[1].sem_op = 1;
    sb[1].sem_flg = 0;
    sb[2].sem_num = 1;
    sb[2].sem_op = -1;
    sb[2].sem_flg = 0;
    if(semop(semid, sb, 3) == -1)
    {
      perror("Semaphore op failed\n");
      exit(1);
    }
  }
  return 0;
}

msgPtr findMessage(char *addr){
  // Find the first message of type 1 in the shared memory
  // Parameter passed will always be address to first block in shared memory
  msgPtr msg;
  msg = (msgPtr) addr;
  void *endPtr = addr + SHMSIZE*sizeof(char);
  while((void *)msg < endPtr && msg->type != getpid()){
    // Increment by size of a message
    msg += sizeof(struct message);
  }
  if ((void *)msg > endPtr)
    return NULL;

  return msg;
}

msgPtr findFreeMemory(char *addr){
  // Find memory block with message type 0
  // Param passed will always be address to first block in shared memory
  msgPtr msg;
  msg = (msgPtr) addr;
  void *endPtr = addr + SHMSIZE*sizeof(char);
  while((void *)msg < endPtr && msg->type != 0){
    // Increment by size of a  message
    msg += sizeof(struct message);
  }
  if((void *)msg > endPtr)
    return NULL;

  return msg;
}