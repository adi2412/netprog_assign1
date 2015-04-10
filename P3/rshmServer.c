#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/select.h>
#include "rshmAPI.h"

// Maximum number of servers it can connect with
#define MAXCONN 20
#define MAXRSHM 40
#define MAXPENDING 5

struct sockaddr_in echoServAddr;
/**
 * This is the TCP Server. It has a lot of jobs.
 * 1. Check for messages in the message queue
 *   Do this by checking if the fifo has anything in it
 * 2. Check if there are any new connections
 *   Do this by checking if the socketfd has anything in it
 * 3. Check if any of the existing connections wrote something
 *   Do this by checking if the connfd has anything in it
 * 4. Send messages to the message queue for API to read
 * 5. Keep the shared memory and it's structure updated
 * 6. Send a hello message to everyone on startup. Behave as client
 *
 * 
 * A remote message would have the following fields
 * type- this could by one of the following
 * 1. rshmget- create a shared memory and update table with rshmid
 * 2. rshmat- update ref count for the particular rshmid
 * 3. rshmdt- decrease ref count for the particular rshmid
 * 4. rshmctl- IPC_RMID command run. update your state tables
 * 5. rshmChanged- Dont know what the fuck to do here
 */

struct rshminfo{
  int rshmid;
  key_t key;
  int shmid;
  void *addr;
  int ref_count;
  struct sockaddr_in *remote_addrs;
};

struct remoteMsg{
  int type;
  int rshmid;
  key_t key;
  size_t size;
  int data;
  struct sockaddr_in myAddr;
};

int connectToOthers(int * sockets)
{
  struct sockaddr_in server;
  char address[25];
  int i;
  i = 0;
  // Connect to all the servers
  printf("Enter the IP and port number of other servers separated by spaces. To stop entering addresses, simply press CTRL+D\n");
  while(fgets(address, 25, stdin) != NULL && i < MAXCONN){
    // Connect to the port.
    sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
    char *ipAddr = strtok(address, " ");
    char *portNum = strtok(NULL, " ");
    if(ipAddr != NULL && portNum != NULL)
    {
      server.sin_family = AF_INET;
      server.sin_addr.s_addr = inet_addr(ipAddr);
      server.sin_port = htons(atoi(portNum));
      printf("%x %d\n", server.sin_addr.s_addr, server.sin_port);
      if(connect(sockets[i++], (struct sockaddr*) &server, sizeof(server)) == -1)
        perror("Connect error");
      else
        printf("Connected\n");
    }
    else
    {
      printf("Please enter valid addresses. To stop entering, press CTRL+D\n");
    }
  }
  printf("\n");
  return i;
}

void sendStateTable(int sockfd, struct rshminfo *rshmTable)
{
  int i = 0;
  while(i < MAXRSHM)
  {
    if(rshmTable[i].rshmid != 0)
    {
      // Send to the server
      send(sockfd, &rshmTable[i], sizeof(struct rshminfo), 0);
    }
    i++;
  }
  // Send an empty state
  struct rshminfo emptyState;
  emptyState.rshmid = 0;
  emptyState.key = 0;
  emptyState.shmid = 0;
  emptyState.addr = NULL;
  emptyState.ref_count = 0;
  emptyState.remote_addrs = NULL;
  send(sockfd, &emptyState, sizeof(struct rshminfo), 0);
}

int updateStateTables(struct rshminfo *rshmTable, int sockfd)
{
  int i = 0, recvMsgSize = 1;
  // Send hello to the socket
  send(sockfd, "Hello", sizeof("Hello"), 0);
  // Get the response from the server
  while(recvMsgSize > 0)
  {
    recvMsgSize = recv(sockfd, &rshmTable[i++], sizeof(struct rshminfo),0);
    if(rshmTable[i-1].rshmid == 0)
    {
      // Empty message
      break;
    }
    printf("Received table\n");
  }
  // The last response received would be a 0 from shutdown
  return i;
}

int maximumFd(int *sockets, int length, int fifo, int servSock)
{
  int maximumSock = 0;
  int i = 0;
  while(i < length)
  {
    if(sockets[i] > maximumSock)
      maximumSock = sockets[i];
    i++;
  }
  if(maximumSock < fifo)
    maximumSock = fifo;
  if(maximumSock < servSock)
    maximumSock = servSock;
  return maximumSock;
}

int sendMessage(msg message, int qid)
{
  int result, length;
  length = sizeof(msg) - sizeof(long);
  if((result = msgsnd(qid, &message, length, 0)) == -1)
  {
    perror("Message queue send");
  }
  return result;
}

msg receiveMessage(int qid)
{
  msg rcvMsg;
  int length, result;
  length = sizeof(msg) - sizeof(long);
  if((result = msgrcv(qid, &rcvMsg, length, -5, 0)) == -1)
  {
    perror("Message queue receive");
  }
  return rcvMsg;
}

int shmByKeyExists(key_t key, struct rshminfo *rshmTable)
{
  int i = 0;
  while(i < MAXRSHM)
  {
    if(key == rshmTable[i].key)
      return rshmTable[i].rshmid;
    i++;
  }
  return 0;
}

int shmByRshmidExists(int rshmid, struct rshminfo *rshmTable)
{
  int i = 0;
  while(i < MAXRSHM)
  {
    if(rshmid == rshmTable[i].rshmid)
      return 1;
    i++;
  }
  return 0;
}

int getSHMByRshm(int rshmid, struct rshminfo *rshmTable)
{
  int i = 0;
  while(i < MAXRSHM)
  {
    if(rshmid == rshmTable[i].rshmid)
      return rshmTable[i].shmid;
    i++;
  }
  return 0;
}

struct rshminfo * getInfoByRshm(int rshmid, struct rshminfo *rshmTable)
{
  int i = 0;
  while(i < MAXRSHM)
  {
    if(rshmid == rshmTable[i].rshmid){
      return &rshmTable[i];
    }
    i++;
  }
  printf("No such record %d\n", i);
  return NULL;
}

void * getAddrByRshm(int rshmid, struct rshminfo *rshmTable)
{
  int i = 0;
  while(i < MAXRSHM)
  {
    if(rshmid == rshmTable[i].rshmid)
      return rshmTable[i].addr;
    i++;
  }
  return NULL;
}

int createSharedMem(key_t key, size_t size)
{
  int shmid = shmget(key, size, IPC_CREAT | 0666);
  if(shmid == -1)
    perror("Shared memory");
  return shmid;
}

int generateRandomNum()
{
  return (rand() % 90000 + 10000);
}

int addNewRSHM(int rshmid, key_t key, int shmid, struct rshminfo *rshmTable)
{
  // Find an empty segment in the array
  int i = 0;
  while(i < MAXRSHM && rshmTable[i].rshmid != 0){
    ++i;
  }

  // Update this entry
  rshmTable[i].rshmid = rshmid;
  rshmTable[i].key = key;
  rshmTable[i].shmid = shmid;
  rshmTable[i].addr = NULL;
  rshmTable[i].ref_count = 0;
  rshmTable[i].remote_addrs = calloc(MAXCONN,sizeof(struct sockaddr_in));
  printf("Updated entry %d with %d\n", i, rshmid);
  // When you add, make sure you attach to the shared memory
  void *address;
  address = shmat(shmid, 0, 0);
  if(address == (void *) -1)
    perror("Attachment error");
  rshmTable[i].addr = address;
  printf("Attached to %p", address);
  return 0;
}

void * attachSharedMem(int rshmid, void *addr, struct rshminfo *rshmTable)
{
  void *address;
  int shmid = getSHMByRshm(rshmid, rshmTable);
  if(shmid){
    address = shmat(shmid, addr, 0);
    if(address == (void *) -1)
      perror("Attachment error");
    return address;
  }
  return NULL;
}

int detachSharedMem(int rshmid, void *addr, struct rshminfo *rshmTable)
{
  void *address = getAddrByRshm(rshmid, rshmTable);
  return shmdt(address);
}

int findAddress(struct sockaddr_in address, struct sockaddr_in *remoteAddrs)
{
  int i = 0;
  while(remoteAddrs[i].sin_family)
  {
    if((remoteAddrs[i].sin_addr.s_addr == address.sin_addr.s_addr) && (remoteAddrs[i].sin_port == address.sin_port))
      return i;
    i++;
  }
  return 0;
}

int removeSharedMem(int rshmid, int cmd, struct rshminfo *rshmTable)
{
  int shmid = getSHMByRshm(rshmid, rshmTable);
  shmctl(shmid, IPC_RMID, 0);
  return 0;
}

int updateTable(int type, int rshmid, void *addr, struct rshminfo *rshmTable)
{
  struct rshminfo *rshmRecord = NULL;
  if(type == 0)
  {
    // Attach
    rshmRecord = getInfoByRshm(rshmid, rshmTable);
    rshmRecord->addr = addr;
    rshmRecord->ref_count++;
    // TODO: remote address
    rshmRecord->remote_addrs[rshmRecord->ref_count-1] = echoServAddr;
    printf("Table updated with %d, %d and %d\n", rshmRecord->remote_addrs[rshmRecord->ref_count-1].sin_family, rshmRecord->remote_addrs[rshmRecord->ref_count-1].sin_addr.s_addr, rshmRecord->remote_addrs[rshmRecord->ref_count-1].sin_port);
  }
  else if(type == 1)
  {
    // Detach
    rshmRecord = getInfoByRshm(rshmid, rshmTable);
    rshmRecord->ref_count--;
    // TODO: remote address
    // Find address in the array
    int index;
    index = findAddress(echoServAddr, rshmRecord->remote_addrs);
    if(index != 0)
    {
      // Remove
      rshmRecord->remote_addrs[index].sin_family = 0;
      rshmRecord->remote_addrs[index].sin_port = 0;
    }
  }
  else if(type == 2)
  {
    // Remove
    rshmRecord = getInfoByRshm(rshmid, rshmTable);
    rshmRecord->rshmid = 0;
    rshmRecord->key = 0;
    rshmRecord->shmid = 0;
    rshmRecord->addr = NULL;
    rshmRecord->ref_count = 0;
    rshmRecord->remote_addrs = NULL;
  }
  return 0;
}

msg handleMessage(msg incomingMsg, struct rshminfo *rshmTable)
{
  if(incomingMsg.mtype == 1)
  {
    // rshmget call
    int rshmid, shmid;
    if((rshmid = shmByKeyExists(incomingMsg.key, rshmTable)))
    {
      incomingMsg.rshmid = rshmid;
      incomingMsg.resp = rshmid;
      return incomingMsg;
    }
    else
    {
      // Create shared memory
      shmid = createSharedMem(incomingMsg.key, incomingMsg.size);
      rshmid = generateRandomNum();
      // Add new entry to state table
      // I should do this below but then I don't have shmid later
      addNewRSHM(rshmid, incomingMsg.key, shmid, rshmTable);
      incomingMsg.rshmid = rshmid;
      incomingMsg.resp = rshmid;
      return incomingMsg;
    }
  }
  else if(incomingMsg.mtype == 2)
  {
    // rshmat call
    void *addr;
    addr = attachSharedMem(incomingMsg.rshmid, incomingMsg.addr, rshmTable);
    // Update entry in table
    updateTable(0, incomingMsg.rshmid, addr, rshmTable);
    printf("Attached to %p\n",addr);
    incomingMsg.addr = addr;
    incomingMsg.resp = 0;
    incomingMsg.shmid = getSHMByRshm(incomingMsg.rshmid, rshmTable);
    return incomingMsg;
  }
  else if(incomingMsg.mtype == 3)
  {
    // rshmdt call
    detachSharedMem(incomingMsg.rshmid, incomingMsg.addr, rshmTable);
    // Update entry in table
    updateTable(1, incomingMsg.rshmid, NULL, rshmTable);
    incomingMsg.resp = 0;
    incomingMsg.shmid = getSHMByRshm(incomingMsg.rshmid, rshmTable);
    return incomingMsg;
  }
  else if(incomingMsg.mtype == 4)
  {
    // rshmctl call
    // Always a remove call only
    removeSharedMem(incomingMsg.rshmid, incomingMsg.cmd, rshmTable);
    // Update entry in table
    updateTable(2, incomingMsg.rshmid, NULL, rshmTable);
    incomingMsg.resp = 0;
    return incomingMsg;
  }
  else if(incomingMsg.mtype == 5)
  {
    // rshmChanged call
    // There is data in shared memory
    incomingMsg.resp = 0;
    return incomingMsg;
  }
  return incomingMsg;
}

void sendRemoteMessage(struct remoteMsg sendMsg, int *sockets, int length)
{
  int j = 0;
  for(;j<length; j++)
  {
    send(sockets[j], &sendMsg, sizeof(sendMsg), 0);
  }
}

void sendMsgToRemoteServers(msg incomingMsg, int *sockets, int length, int rshmid, struct rshminfo *rshmTable)
{
  struct remoteMsg sendMsg;
  if(incomingMsg.mtype == 1)
  {
    // rshmget call
    int rid;
    if((rid = shmByRshmidExists(rshmid, rshmTable)) == 0)
    {
      return;
    }
    else
    {
      // Send message to others about new memory
      sendMsg.type = 1;
      sendMsg.rshmid = rshmid;
      sendMsg.key = incomingMsg.key;
      sendMsg.size = incomingMsg.size;
      sendMsg.data = 0;
    }
  }
  else if(incomingMsg.mtype == 2)
  {
    // rshmat call
    // Send message to increase ref count
    sendMsg.type = 2;
    sendMsg.rshmid = rshmid;
    sendMsg.data = 0;
  }
  else if(incomingMsg.mtype == 3)
  {
    // rshmdt call
    // Send message to decrease ref count
    sendMsg.type = 3;
    sendMsg.rshmid = rshmid;
    sendMsg.data = 0;
  }
  else if(incomingMsg.mtype == 4)
  {
    // rshmctl call
    sendMsg.type = 4;
    sendMsg.rshmid = rshmid;
    sendMsg.data = 0;
  }
  else if(incomingMsg.mtype == 5)
  {
    // rshmChanged call
    sendMsg.type = 5;
    sendMsg.rshmid = rshmid;
    void *addr = getAddrByRshm(rshmid, rshmTable);
    // sendMsg.data = malloc(1024*sizeof(char));
    int data = *(int *)addr;
    sendMsg.data = data;
  }
  sendMsg.myAddr = echoServAddr;
  sendRemoteMessage(sendMsg, sockets, length);
}

int handleRemoteMessage(struct remoteMsg tcpMsg, struct rshminfo *rshmTable)
{
  if(tcpMsg.type == 1)
  {
    // New memory segment
    // Create shared memory
    int shmid;
    shmid = createSharedMem(tcpMsg.key, tcpMsg.size);
    addNewRSHM(tcpMsg.rshmid, tcpMsg.key, shmid, rshmTable);
  }
  else if(tcpMsg.type == 2)
  {
    // rshmat call
    // Increase refcount
    struct rshminfo *rshmRecord = NULL;
    rshmRecord = getInfoByRshm(tcpMsg.rshmid, rshmTable);
    if(rshmRecord)
    {
      rshmRecord->ref_count++;
      // TODO: Remote addrs?
      rshmRecord->remote_addrs[rshmRecord->ref_count - 1] = tcpMsg.myAddr;
    }
    else
    {
      printf("No such shm");
    }
  }
  else if(tcpMsg.type == 3)
  {
    // rshmdt call
    // Decrease ref_count
    struct rshminfo *rshmRecord = NULL;
    rshmRecord = getInfoByRshm(tcpMsg.rshmid, rshmTable);
    rshmRecord->ref_count--;
    // TODO: Remote addrs?
    // Find address
    int index;
    index = findAddress(tcpMsg.myAddr, rshmRecord->remote_addrs);
    if(index != 0)
    {
      // Remove
      rshmRecord->remote_addrs[index].sin_family = 0;
      rshmRecord->remote_addrs[index].sin_port = 0;
    }
  }
  else if(tcpMsg.type == 4)
  {
    // rshmctl call
    // Remove shared memory
    removeSharedMem(tcpMsg.rshmid, IPC_RMID, rshmTable);
    // Update table
    struct rshminfo *rshmRecord = NULL;
    rshmRecord = getInfoByRshm(tcpMsg.rshmid, rshmTable);
    rshmRecord->rshmid = 0;
    rshmRecord->key = 0;
    rshmRecord->shmid = 0;
    rshmRecord->addr = NULL;
    rshmRecord->ref_count = 0;
    rshmRecord->remote_addrs = NULL;
  }
  else if(tcpMsg.type == 5)
  {
    // rshmChanged call
    // Update shared memory with data
    struct rshminfo *rshmRecord = NULL;
    rshmRecord = getInfoByRshm(tcpMsg.rshmid, rshmTable);
    if(rshmRecord->addr == NULL)
    {
      // No client from my side has attached yet
      // TCP attaches to the shared memory
      printf("Address is null. can't add data\n"); 
    }
    else
    {
      int data;
      data = tcpMsg.data;
      printf("Data received: %d\n", data);
    }
  }
  return 0;
}

void removeSocket(int j, int *sockets, int length)
{
  int i = j;
  for(;i<=length;i++)
  {
    sockets[i] = sockets[i+1];
  }
}

int numConns, servSock, qid;
int sockets[MAXCONN];

void sigHandler(int signo)
{
  int i = 0;
  for(;i<numConns;++i)
  {
    close(servSock);
  }
  msgctl(qid, IPC_RMID, 0);
  exit(0);
}

int main(int argc, char *argv[])
{
  signal(SIGKILL, sigHandler);
  signal(SIGINT, sigHandler);
  signal(SIGQUIT, sigHandler);
  struct sockaddr clientAddr;
  struct rshminfo *rshmInfo = malloc(MAXRSHM * sizeof(struct rshminfo));
  int numRSHM, fifo, maxfd;
  unsigned short echoServPort;
  unsigned int clntLen;
  msg incomingMsg;
  struct remoteMsg tcpMsg;
  key_t key;
  fd_set allset, rset;
  if (argc != 2)
  {
     fprintf(stderr, "Usage:  %s <Server Port>\n", argv[0]);
     exit(1);
  }

  echoServPort = atoi(argv[1]);

  if ((servSock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
     perror("socket() failed");

  memset(&echoServAddr, 0, sizeof(echoServAddr));

  echoServAddr.sin_family = AF_INET;

  echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  echoServAddr.sin_port = htons(echoServPort);

  if (bind(servSock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
    perror("bind() failed");

  if (listen(servSock, MAXPENDING) < 0)
      perror("listen() failed");

  // Create the message queue
  fopen(KEY, "w+");
  key = ftok(KEY, '4');
  qid = msgget(key, IPC_CREAT | 0666);
  if(qid == -1)
    perror("Message queue");

  // Handle the connection to other servers
  numConns = connectToOthers(sockets);

  // Send hello message to server. It only need ask one of them
  // for the state tables since all keep their state tables
  // updated.
  if(numConns > 0)
  {
    // Update the state table
    // Just going to assume the first one will give me the 
    // updated state table
    numRSHM = updateStateTables(rshmInfo, sockets[0]);
  }
  else
  {
    // First TCP server. No state table updates needed.
  }

  // Create the fifo
  mkfifo(FIFO, 0666);
  fifo = open(FIFO, O_RDWR | O_NONBLOCK);

  // Find maxfd
  maxfd = maximumFd(sockets, numConns, fifo, servSock);

  FD_ZERO(&allset);
  FD_SET(fifo, &allset);
  FD_SET(servSock, &allset);
  int i = 0;
  while(i < numConns)
  {
    FD_SET(sockets[i++], &allset);
  }

  for( ; ; )
  {
    rset = allset;
    int ncnt;
    printf("Waiting on fds\n");
    ncnt = select(maxfd + 1, &rset, NULL, NULL, NULL);
    if(FD_ISSET(servSock, &rset))
    {
      // New connection
      printf("New connection\n");
      clntLen = sizeof(struct sockaddr);
      if(numConns < MAXCONN)
      {
        sockets[numConns++] = accept(servSock, &clientAddr, &clntLen);
        FD_SET(sockets[numConns-1], &allset);
        if(sockets[numConns-1] > maxfd)
          maxfd = sockets[numConns-1];

      }
      else
      {
        printf("Too many connections\n");
      }
      ncnt--;
    }
    if(FD_ISSET(fifo, &rset) && ncnt)
    {
      // There are messages in the message queue
      // Clear the fifo
      char a;
      int num;
      num = read(fifo, &a, sizeof(char));
      if(num > 0)
      {
        msg outMsg;
        printf("Data on message queue\n");
        incomingMsg = receiveMessage(qid);
        outMsg = handleMessage(incomingMsg, rshmInfo);
        outMsg.mtype = 6;
        sendMessage(outMsg, qid);
        // send messages to other servers
        sendMsgToRemoteServers(incomingMsg, sockets, numConns, outMsg.rshmid, rshmInfo);
      }
      ncnt--;
    }
    int j = 0;
    for(;j<numConns && ncnt > 0; j++)
    {
      if(FD_ISSET(sockets[j], &rset))
      {
        // A message from a TCP connection
        printf("Message from TCP connection\n");
        int recvSize = 0;
        recvSize = recv(sockets[j], &tcpMsg, sizeof(tcpMsg), 0);
        if(recvSize > 0)
        {
          char *helloMsg = "Hello";
          if(!strcmp((char *)&tcpMsg, helloMsg))
          {
            // Received a hello. Send state table
            sendStateTable(sockets[j], rshmInfo);
          }
          else
          {
            // Handle the message
            handleRemoteMessage(tcpMsg, rshmInfo);
          }
        }
        else
        {
          numConns--;
          FD_CLR(sockets[j], &allset);
          removeSocket(j, sockets, numConns);
        }
        ncnt--;
      }
    }
  }
}