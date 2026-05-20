#include <iostream>
#include "cond.c"
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signal.h>

enum State{
    STATE_EMPTY,
    STATE_DEMON_INSIDE,
    STATE_NOMAD_INSIDE
};

std::string ftokFile="manager.cpp";

static int* H=nullptr;
static int  H_shmid=-1;
static Cond_t CV;

void cleanup()
{
    if(H!=nullptr) shmdt(static_cast<void*>(H));
    if(H_shmid!=-1) shmctl(H_shmid,IPC_RMID,NULL);
    cond_destroy(&CV);
}

void sigint_handler(int)
{
    cleanup();
    exit(0);
}

int main(){
    signal(SIGINT,sigint_handler);

    key_t token1=ftok(ftokFile.c_str(),'M');
    key_t token2=ftok(ftokFile.c_str(),'N');
    key_t H_Key=ftok(ftokFile.c_str(),'O');

    CV=cond_create(token1,token2);
    cond_init(&CV);

    H_shmid=shmget(H_Key,sizeof(int)*3,IPC_CREAT|0666);
    if(H_shmid==-1){perror("shmget H");exit(1);}

    H=static_cast<int*>(shmat(H_shmid,NULL,0));
    if(H==(int*)-1){perror("shmat H");exit(1);}

    H[0]=static_cast<int>(STATE_EMPTY);
    H[1]=0;
    H[2]=0;

    std::cout<<"Manager started. Press Enter to terminate...\n>";
    std::cin.get();

    cleanup();
    return 0;
}