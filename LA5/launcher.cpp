#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include "common.h"

using namespace std;

int *g_T=nullptr;
int g_semId=-1;
int g_managerPid=-1, g_timerPid=-1;
vector<pid_t> g_childPids;

void sigint_handler(int) {
  if (g_timerPid>0)   kill(g_timerPid,   SIGINT);
  if (g_managerPid>0) kill(g_managerPid, SIGINT);
  for (pid_t pid : g_childPids)
    if (pid>0) kill(pid, SIGKILL);
  exit(0);
}

int main() {
  key_t keyTSeg=ftok(".", 'T');
  key_t keySem=ftok(".", 'S');

  if (keyTSeg==-1||keySem==-1) {
    cerr<<"launcher: ftok failed"<<endl;
    return 1;
  }

  int shmTId=shmget(keyTSeg, T_SIZE*sizeof(int), 0666);
  if (shmTId<0) {
    perror("launcher: shmget T");
    return 1;
  }

  g_T=(int *)shmat(shmTId, nullptr, 0);
  if (g_T==(int *)-1) {
    cerr<<"launcher: shmat T failed"<<endl;
    return 1;
  }

  g_semId=semget(keySem, 4, 0666);
  if (g_semId<0) {
    perror("launcher: semget");
    return 1;
  }

  g_managerPid=g_T[3];
  g_timerPid=g_T[4];

  signal(SIGINT, sigint_handler);

  cout<<"["<<g_T[0]<<"] Launcher Ready"<<endl;

  ifstream infile(INPUT_FILE);
  if (!infile.is_open()) {
    cerr<<"launcher: could not open "<<INPUT_FILE<<endl;
    return 1;
  }

  int procSerial=0;
  int localTime=0;
  string line;

  while (getline(infile, line)) {
    istringstream iss(line);
    int arrivalTime;
    if (!(iss>>arrivalTime)||arrivalTime<0)
      break;

    int priority;
    if (!(iss>>priority))
      break;

    int burstData[21];
    for (int i=0; i<21; i++) {
      if (!(iss>>burstData[i])) {
        cerr<<"launcher: error reading burst data for process "<<procSerial<<endl;
        break;
      }
    }

    while (true) {
      usleep(DELTA);
      sem_wait_zero(g_semId, SEM_SYNC);
      localTime=g_T[0];
      if (localTime>=arrivalTime)
        break;
    }

    pid_t pid=fork();
    if (pid<0) {
      perror("launcher: fork");
      continue;
    } else if (pid==0) {
      infile.close();

      string serialStr=to_string(procSerial);
      string priorityStr=to_string(priority);
      vector<string> burstStrs(21);
      for (int i=0; i<21; i++)
        burstStrs[i]=to_string(burstData[i]);

      char *args[25];
      args[0]=(char *)"./process";
      args[1]=(char *)serialStr.c_str();
      args[2]=(char *)priorityStr.c_str();
      for (int i=0; i<21; i++)
        args[3+i]=(char *)burstStrs[i].c_str();
      args[24]=nullptr;

      execvp("./process", args);
      perror("launcher: execvp");
      exit(1);
    } else {
      g_childPids.push_back(pid);
      procSerial++;
    }
  }

  infile.close();

  for (pid_t cpid : g_childPids)
    waitpid(cpid, nullptr, 0);

  cout<<"\t\t\t[Launcher] All processes exited"<<endl;

  if (g_timerPid>0)   kill(g_timerPid,   SIGINT);
  if (g_managerPid>0) kill(g_managerPid, SIGINT);

  shmdt(g_T);
  return 0;
}
