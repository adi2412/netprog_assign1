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
  rshmid = rshmget(key, 1024);
  printf("RSHMID: %d\n", rshmid);

  // Call the RSHMAT API
  addr = rshmat(rshmid, NULL);
  // Write something in shared memory
  strcpy(addr, "Hello");
  // Alert the server
  rshmChanged(rshmid);
  // Call the RSHMDT API
  rshmdt(rshmid, addr);

  // Call the RSHMCTL API
  rshmCtl(rshmid, IPC_RMID);

  // Call the RSHMGET API again
  rshmid = rshmget(key, 1024);
  printf("RSHMID: %d\n", rshmid);
  return 0;
}