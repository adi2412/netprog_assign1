#define KEY "/tmp/11042_11823_p3_msgq.cfg"
#define FIFO "/tmp/11042_11823_p3_fifo"

struct msgBuf{
  long mtype;
  key_t key;
  size_t size;
  int rshmid;
  int shmid;
  void *addr;
  int cmd;
  int resp;
};

typedef struct msgBuf msg;

int rshmget(int key, size_t size);
void * rshmat(int rshmid, void *addr);
int rshmdt(int rshmid, void *addr);
int rshmCtl(int rshmid, int cmd);
void rshmChanged(int rshmid);