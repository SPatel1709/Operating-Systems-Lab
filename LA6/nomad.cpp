#include <iostream>
#include "cond.c"
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

enum State{
    STATE_EMPTY,
    STATE_DEMON_INSIDE,
    STATE_NOMAD_INSIDE
};

std::string ftokFile="manager.cpp";

static int*   H=nullptr;
static int    H_shmid=-1;
static Cond_t CV;

void cleanup()
{
    if(H!=nullptr) shmdt(static_cast<void*>(H));
}

void sigint_handler(int)
{
    cleanup();
    exit(0);
}

void nomad(int id)
{
    while(1)
    {
        cond_lock(&CV);
        std::cout<<"\t\t\tNomad "<<id<<" arrives (N_CNT = "<<(H[0]==STATE_NOMAD_INSIDE?H[1]:0);
        std::cout<<", D_CNT = "<<(H[0]==STATE_DEMON_INSIDE?H[1]:0);
        std::cout<<", state = "<<(H[0]==STATE_EMPTY?"EMPTY":H[0]==STATE_DEMON_INSIDE?"D_INSIDE":"N_INSIDE");
        std::cout<<")"<<std::endl;

        while(H[0]==STATE_DEMON_INSIDE)
        {
            ++H[2];
            std::cout<<"\t\t\tNomad "<<id<<" waits (NW_CNT = "<<H[2]<<")"<<std::endl;
            cond_wait(&CV);
            --H[2];
        }

        ++H[1];
        if(H[0]==STATE_EMPTY)
        {
            H[0]=STATE_NOMAD_INSIDE;
            std::cout<<"\t\t\tNomad "<<id<<" enters [house empty] (N_CNT = "<<H[1]<<", D_CNT = 0, state = N_INSIDE)"<<std::endl;
            if(H[2]>0) cond_broadcast(&CV);
        }
        else
        {
            std::cout<<"\t\t\tNomad "<<id<<" enters [other nomads present] (N_CNT = "<<H[1]<<", D_CNT = 0, state = N_INSIDE)"<<std::endl;
        }
        cond_unlock(&CV);

        usleep(500000+rand()%500000);

        cond_lock(&CV);
        --H[1];
        if(H[1]==0)
        {
            H[0]=STATE_EMPTY;
            std::cout<<"\t\t\tNomad "<<id<<" leaves (N_CNT = 0, D_CNT = 0, state = EMPTY)"<<std::endl;
            if(H[2]>0) cond_broadcast(&CV);
        }
        else
        {
            std::cout<<"\t\t\tNomad "<<id<<" leaves (N_CNT = "<<H[1]<<", D_CNT = 0, state = N_INSIDE)"<<std::endl;
        }
        cond_unlock(&CV);

        usleep(500000+rand()%500000);
    }
}

int main(int argc,char* argv[])
{
    int n=(argc>1)?atoi(argv[1]):10;

    key_t token1=ftok(ftokFile.c_str(),'M');
    key_t token2=ftok(ftokFile.c_str(),'N');
    key_t H_Key=ftok(ftokFile.c_str(),'O');

    CV=cond_create(token1,token2);

    H_shmid=shmget(H_Key,sizeof(int)*3,0666);
    if(H_shmid==-1){
        error("shmget H");
    }

    H=static_cast<int*>(shmat(H_shmid,NULL,0));
    if(H==(int*)-1){
        error("shmat H");
    }

    signal(SIGINT,sigint_handler);

    for(int i=0;i<n;i++)
    {
        pid_t pid=fork();
        if(pid==0)
        {
            srand(getpid());
            nomad(i);
            exit(0);
        }
    }

    for(int i=0;i<n;i++)
        wait(NULL);

    cleanup();
    return 0;
}
