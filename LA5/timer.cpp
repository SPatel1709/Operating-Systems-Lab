#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include "common.h"

using namespace std;

int g_timerRunning=1;

void sigint_handler(int){ g_timerRunning=0; }

int main()
{
  key_t keyTSeg=ftok(".", 'T');
  key_t keyPcb=ftok(".", 'P');
  key_t keySem=ftok(".", 'S');

  if (keyTSeg==-1 || keyPcb==-1 || keySem==-1)
  {
    cerr<<"timer: ftok failed"<<endl;
    return 1;
  }

  int shmTId=shmget(keyTSeg,T_SIZE*sizeof(int),0666);
  int shmPcbId=shmget(keyPcb,PCB_SHM_SIZE*sizeof(int),0666);
  if (shmTId<0 || shmPcbId<0)
  {
    perror("timer: shmget");
    return 1;
  }

  int *T=(int *)shmat(shmTId,nullptr,0);
  int *PCB=(int *)shmat(shmPcbId,nullptr,0);
  if (T==(int *)-1 || PCB==(int *)-1)
  {
    cerr<<"timer: shmat failed"<<endl;
    return 1;
  }

  int semId=semget(keySem,4,0666);
  if (semId<0)
  {
    perror("timer: semget");
    return 1;
  }

  T[4]=getpid();
  cout<<"Timer: Started. PID="<<getpid()<<endl;
  signal(SIGINT,sigint_handler);

  sem_P(semId,SEM_SYNC);
  sem_V(semId,SEM_SYNC);

  while(g_timerRunning)
  {
    usleep(DELTA+DELTA2);
    if(!g_timerRunning)
      break;

    T[0]++;

    sem_P(semId, SEM_SYNC);

    int running=T[1];
    int nextInt=T[2];
    int currentTime=T[0];

    if (running>=0 && nextInt>=0 && currentTime>=nextInt)
    {
      int procPid=PCB[running * PCB_ENTRY + 1];

      if (procPid>0)
        kill(procPid, SIGUSR1);
    }

    usleep(DELTA2);

    sem_V(semId, SEM_SYNC);
    if (!g_timerRunning)
      break;
  }

  shmdt(T);
  shmdt(PCB);
  cout<<"Timer: Exiting."<<endl;
  return 0;
}
