#include <iostream>
#include <string>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include "common.h"

using namespace std;

int *g_T=nullptr, *g_RQ=nullptr, *g_PCB=nullptr;
int g_semId = -1;

int g_mySerial=-1, g_myPriority=-1;
int g_bursts[21];
int g_currentBurst=0,g_remainingBurstTime=0;

int g_interrupted = 0;

void rq_enqueue(int pid)
{
  sem_P(g_semId, SEM_RQ);
  int back = g_RQ[QUEUE_SIZE + 2];
  g_RQ[back] = pid;
  g_RQ[QUEUE_SIZE + 2] = (back + 1) % (QUEUE_SIZE + 1);
  sem_V(g_semId, SEM_RQ);
}

int rq_dequeue()
{
  sem_P(g_semId, SEM_RQ);
  int front = g_RQ[QUEUE_SIZE + 1], back = g_RQ[QUEUE_SIZE + 2];
  if (front == back)
  {
    sem_V(g_semId, SEM_RQ);
    return -1;
  }
  int pid = g_RQ[front];
  g_RQ[QUEUE_SIZE + 1] = (front + 1) % (QUEUE_SIZE + 1);
  sem_V(g_semId, SEM_RQ);
  return pid;
}

int get_quantum(int pri)
{
  if (pri == 0)
    return 10;
  if (pri == 1)
    return 5;
  return 2;
}

void schedule_next()
{
  int next = rq_dequeue();
  if (next < 0)
  {
    sem_P(g_semId, SEM_T);
    g_T[1] = -1;
    g_T[2] = -1;
    sem_V(g_semId, SEM_T);
    cout << "[" << g_T[0] << "] CPU goes idle" << endl;
    return;
  }
  int q = get_quantum(g_PCB[next * PCB_ENTRY + 2]);
  sem_P(g_semId, SEM_T);
  int ct = g_T[0];
  g_T[1] = next;
  g_T[2] = ct + q;
  sem_V(g_semId, SEM_T);
  sem_P(g_semId, SEM_PCB);
  int old = g_PCB[next * PCB_ENTRY + 3];
  g_PCB[next * PCB_ENTRY + 3] = STATE_RUNNING;
  sem_V(g_semId, SEM_PCB);
  if (old == STATE_READY)
    cout << "[" << ct << "] Process " << next
         << ": Going from READY to RUNNING with next interrupt time = " << ct + q << endl;
}

void sigusr1_handler(int) { g_interrupted = 1; }

void sync_with_timer()
{
  usleep(DELTA);
  sem_wait_zero(g_semId, SEM_SYNC);
}

int main(int argc, char *argv[])
{
  if (argc < 24)
  {
    cerr << "process: need 24 args, got " << argc << endl;
    return 1;
  }

  g_mySerial = stoi(string(argv[1]));
  g_myPriority = stoi(string(argv[2]));
  for (int i = 0; i < 21; i++)
    g_bursts[i] = stoi(string(argv[3 + i]));

  key_t keyTSeg = ftok(".", 'T');
  key_t keyRq = ftok(".", 'R');
  key_t keyPcb = ftok(".", 'P');
  key_t keySem = ftok(".", 'S');
  if (keyTSeg == -1 || keyRq == -1 || keyPcb == -1 || keySem == -1)
  {
    cerr << "process: ftok failed" << endl;
    return 1;
  }

  int shmTId = shmget(keyTSeg, T_SIZE * sizeof(int), 0666);
  int shmRqId = shmget(keyRq, RQ_SHM_SIZE * sizeof(int), 0666);
  int shmPcbId = shmget(keyPcb, PCB_SHM_SIZE * sizeof(int), 0666);
  if (shmTId < 0 || shmRqId < 0 || shmPcbId < 0)
  {
    perror("process: shmget");
    return 1;
  }

  g_T = (int *)shmat(shmTId, nullptr, 0);
  g_RQ = (int *)shmat(shmRqId, nullptr, 0);
  g_PCB = (int *)shmat(shmPcbId, nullptr, 0);
  if (g_T == (int *)-1 || g_RQ == (int *)-1 || g_PCB == (int *)-1)
  {
    cerr << "process: shmat failed" << endl;
    return 1;
  }

  g_semId = semget(keySem, 4, 0666);
  if (g_semId < 0)
  {
    perror("process: semget");
    return 1;
  }

  signal(SIGUSR1, sigusr1_handler);

  sem_P(g_semId, SEM_PCB);
  g_PCB[g_mySerial * PCB_ENTRY + 0] = g_mySerial;
  g_PCB[g_mySerial * PCB_ENTRY + 1] = getpid();
  g_PCB[g_mySerial * PCB_ENTRY + 2] = g_myPriority;
  g_PCB[g_mySerial * PCB_ENTRY + 3] = STATE_READY;
  sem_V(g_semId, SEM_PCB);

  cout << "[" << g_T[0] << "] Process " << g_mySerial
       << ": Arrival with priority = " << g_myPriority << endl;

  rq_enqueue(g_mySerial);

  sem_P(g_semId, SEM_T);
  int runningSerial = g_T[1];
  sem_V(g_semId, SEM_T);

  if (runningSerial == -1)
  {
    schedule_next();
  }
  else
  {
    int runningPri = g_PCB[runningSerial * PCB_ENTRY + 2];
    if (g_myPriority <= runningPri)
    {
      sem_P(g_semId, SEM_PCB);
      g_PCB[runningSerial * PCB_ENTRY + 3] = STATE_READY;
      sem_V(g_semId, SEM_PCB);
      rq_enqueue(runningSerial);

      int next = rq_dequeue();
      int q = get_quantum(g_PCB[next * PCB_ENTRY + 2]);
      sem_P(g_semId, SEM_T);
      int now = g_T[0];
      g_T[1] = next;
      g_T[2] = now + q;
      sem_V(g_semId, SEM_T);
      sem_P(g_semId, SEM_PCB);
      g_PCB[next * PCB_ENTRY + 3] = STATE_RUNNING;
      sem_V(g_semId, SEM_PCB);
      cout << "[" << now << "] Process " << next
           << ": Going from READY to RUNNING with next interrupt time = " << now + q << endl;

      int runningPid = g_PCB[runningSerial * PCB_ENTRY + 1];
      if (runningPid > 0)
        kill(runningPid, SIGUSR1);
    }
  }

  if (g_mySerial == 0)
    sem_V(g_semId, SEM_SYNC);

  g_currentBurst = 0;
  g_remainingBurstTime = g_bursts[0];

  while (true)
  {
    sync_with_timer();

    int ct = g_T[0];
    int myState = g_PCB[g_mySerial * PCB_ENTRY + 3];

    if (myState == STATE_EXITED)
      break;

    if (myState != STATE_RUNNING)
      g_interrupted = 0;

    if (myState == STATE_RUNNING)
    {
      g_remainingBurstTime--;

      if (g_remainingBurstTime <= 0)
      {
        cout << "[" << ct << "] Process " << g_mySerial
             << ": CPU burst " << g_currentBurst / 2 << " complete" << endl;

        if (g_currentBurst >= 20)
        {
          sem_P(g_semId, SEM_PCB);
          g_PCB[g_mySerial * PCB_ENTRY + 3] = STATE_EXITED;
          sem_V(g_semId, SEM_PCB);
          schedule_next();
          cout << "\t\t\t[" << ct << "] Process " << g_mySerial << ": Exiting" << endl;
          break;
        }
        g_currentBurst++;
        g_remainingBurstTime = g_bursts[g_currentBurst];
        sem_P(g_semId, SEM_PCB);
        g_PCB[g_mySerial * PCB_ENTRY + 3] = STATE_IO;
        sem_V(g_semId, SEM_PCB);
        schedule_next();
      }
      else if (g_interrupted)
      {
        g_interrupted = 0;
        cout << "[" << ct << "] Process " << g_mySerial << ": Interrupted" << endl;

        sem_P(g_semId, SEM_PCB);
        g_PCB[g_mySerial * PCB_ENTRY + 3] = STATE_READY;
        sem_V(g_semId, SEM_PCB);
        rq_enqueue(g_mySerial);

        int next = rq_dequeue();
        if (next >= 0)
        {
          int q = get_quantum(g_PCB[next * PCB_ENTRY + 2]);
          sem_P(g_semId, SEM_T);
          int now = g_T[0];
          g_T[1] = next;
          g_T[2] = now + q;
          sem_V(g_semId, SEM_T);
          sem_P(g_semId, SEM_PCB);
          g_PCB[next * PCB_ENTRY + 3] = STATE_RUNNING;
          sem_V(g_semId, SEM_PCB);
          cout << "[" << ct << "] Process " << g_mySerial
               << ": Context switch to process " << next
               << " with next interrupt time = " << now + q << endl;
        }
      }
    }
    else if (myState == STATE_IO)
    {
      g_remainingBurstTime--;
      if (g_remainingBurstTime <= 0)
      {
        cout << "[" << ct << "] Process " << g_mySerial
             << ": IO burst " << g_currentBurst / 2 << " complete" << endl;
        g_currentBurst++;
        g_remainingBurstTime = g_bursts[g_currentBurst];
        sem_P(g_semId, SEM_PCB);
        g_PCB[g_mySerial * PCB_ENTRY + 3] = STATE_READY;
        sem_V(g_semId, SEM_PCB);
        rq_enqueue(g_mySerial);
        sem_P(g_semId, SEM_T);
        bool idle = (g_T[1] == -1);
        sem_V(g_semId, SEM_T);
        if (idle)
          schedule_next();
      }
    }
  }

  shmdt(g_T);
  shmdt(g_RQ);
  shmdt(g_PCB);
  return 0;
}
