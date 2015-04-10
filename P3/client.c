#include <unistd.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include "rshmAPI.h"

/**
 * Client simply calls the RSHM API
 */

#define SHMKEY "2011A7PS042P"

int main()
{
  int rshmid;
  char *addr;
  key_t key;
  key = ftok(KEY, 'A');
  // Call the RSHMGET API
  printf("Calling rshmget\n");
  rshmid = rshmget(key, 1024);
  printf("RSHMID: %d\n", rshmid);

  // Call the RSHMAT API
  printf("Calling rshmat\n");
  addr = rshmat(rshmid, NULL);
  printf("Attached\n");
  // Write something in shared memory
  printf("Writing data into rshm\n");
  *addr = 6;
  // Alert the server
  printf("Calling rshmchanged\n");
  rshmChanged(rshmid);
  // Call the RSHMDT API
  printf("Calling rshmdt\n");
  rshmdt(rshmid, addr);

  // Call the RSHMCTL API
  printf("Destroying rshm using rshmCtl\n");
  rshmCtl(rshmid, IPC_RMID);

  // Call the RSHMGET API again
  printf("Creating new rshm again using rshmget\n");
  rshmid = rshmget(key, 1024);
  printf("RSHMID: %d\n", rshmid);
  return 0;
}