#ifndef COMMON_H
#define COMMON_H

#include <sys/sem.h>

// Global constants here constexpr makes it same as #define basically marco type so they 
// are compile time constants and not run time. Crazi shit bhaii sab 
static constexpr int MAX_PROC = 100;
static constexpr int QUEUE_SIZE = 1003;
static constexpr int T_SIZE = 5;
static constexpr int RQ_SHM_SIZE = QUEUE_SIZE + 3;
static constexpr int PCB_ENTRY = 4;
static constexpr int PCB_SHM_SIZE = MAX_PROC * PCB_ENTRY;


// udi baba e mat bhulna ki yeh microsecond haai 
static constexpr int DELTA = 50000;   
static constexpr int DELTA2 = 10000;

static constexpr const char *INPUT_FILE = "bursts.txt";

enum SemIdx
{
  SEM_RQ = 0,
  SEM_PCB = 1,
  SEM_T = 2,
  SEM_SYNC = 3
};

enum ProcState
{
  STATE_READY = 0,
  STATE_RUNNING = 1,
  STATE_IO = 2,
  STATE_EXITED = 3
};

union semun
{
  int val;
  struct semid_ds *buf;
  unsigned short *array;
};

// Semaphore operation funcs
static inline void sem_P(int semId, int n)
{
  struct sembuf op = {(unsigned short)n, -1, 0};
  semop(semId, &op, 1);
}

static inline void sem_V(int semId, int n)
{
  struct sembuf op = {(unsigned short)n, 1, 0};
  semop(semId, &op, 1);
}

static inline void sem_wait_zero(int semId, int n)
{
  struct sembuf op = {(unsigned short)n, 0, 0};
  semop(semId, &op, 1);
}

#endif
