#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <algorithm>
#include "common.h"

using namespace std;

int g_shmTId = -1, g_shmRqId = -1, g_shmPcbId = -1, g_semId = -1;

void cleanup_and_exit(int)
{
  if (g_shmTId != -1)
    shmctl(g_shmTId, IPC_RMID, nullptr);
  if (g_shmRqId != -1)
    shmctl(g_shmRqId, IPC_RMID, nullptr);
  if (g_shmPcbId != -1)
    shmctl(g_shmPcbId, IPC_RMID, nullptr);
  if (g_semId != -1)
    semctl(g_semId, 0, IPC_RMID);
  exit(0);
}

int main()
{
  key_t keyTSeg = ftok(".", 'T');
  key_t keyRq = ftok(".", 'R');
  key_t keyPcb = ftok(".", 'P');
  key_t keySem = ftok(".", 'S');

  if (keyTSeg == -1 || keyRq == -1 || keyPcb == -1 || keySem == -1)
  {
    cerr << "manager: ftok failed" << endl;
    return 1;
  }

  g_shmTId = shmget(keyTSeg, T_SIZE * sizeof(int), IPC_CREAT | 0666);
  g_shmRqId = shmget(keyRq, RQ_SHM_SIZE * sizeof(int), IPC_CREAT | 0666);
  g_shmPcbId = shmget(keyPcb, PCB_SHM_SIZE * sizeof(int), IPC_CREAT | 0666);

  if (g_shmTId < 0 || g_shmRqId < 0 || g_shmPcbId < 0)
  {
    perror("manager: shmget");
    return 1;
  }

  int *T = (int *)shmat(g_shmTId, nullptr, 0);
  int *RQ = (int *)shmat(g_shmRqId, nullptr, 0);
  int *PCB = (int *)shmat(g_shmPcbId, nullptr, 0);

  if (T == (int *)-1 || RQ == (int *)-1 || PCB == (int *)-1)
  {
    cerr << "manager: shmat failed" << endl;
    return 1;
  }

  T[0] = 0;
  T[1] = -1;
  T[2] = -1;
  T[3] = getpid();
  T[4] = 0;

  fill(RQ, RQ + RQ_SHM_SIZE, 0);
  fill(PCB, PCB + PCB_SHM_SIZE, 0);

  RQ[QUEUE_SIZE + 1] = RQ[QUEUE_SIZE + 2] = 0;

  shmdt(T);
  shmdt(RQ);
  shmdt(PCB);

  g_semId = semget(keySem, 4, IPC_CREAT | 0666);

  if (g_semId < 0)
  {
    perror("semget");
    return 1;
  }

  semun arg;
  arg.val = 1;
  semctl(g_semId, SEM_RQ, SETVAL, arg);
  semctl(g_semId, SEM_PCB, SETVAL, arg);
  semctl(g_semId, SEM_T, SETVAL, arg);

  arg.val = 0;
  semctl(g_semId, SEM_SYNC, SETVAL, arg);

  cout << "Manager: IPC resources created. PID=" << getpid() << endl;

  signal(SIGINT, cleanup_and_exit);
  while (true)
    pause();

  return 0;
}
