#include <stdio.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct Cond{
    int semid;
    int shmid;
}Cond_t;

#define MTX 0
#define CND 1

void error(const char* msg)
{
    fprintf(stderr,"%s\n",msg);
    exit(EXIT_FAILURE);
}

void P(int semid,int sem_num)
{
    struct sembuf op={(short unsigned int)sem_num,-1,0};
    if(semop(semid,&op,1)==-1)
        error("semop P failed");
}

void V(int semid,int sem_num)
{
    struct sembuf op={(short unsigned int)sem_num,1,0};
    if(semop(semid,&op,1)==-1)
        error("semop V failed");
}

Cond_t cond_create(key_t token1,key_t token2)
{
    Cond_t CV;
    CV.semid=semget(token1,2,IPC_CREAT|0666);
    if(CV.semid==-1) error("semget");
    CV.shmid=shmget(token2,sizeof(int),IPC_CREAT|0666);
    if(CV.shmid==-1) error("shmget");
    return CV;
}

void cond_init(Cond_t* CV)
{
    if(semctl(CV->semid,MTX,SETVAL,1)==-1) error("semctl MTX");
    if(semctl(CV->semid,CND,SETVAL,0)==-1) error("semctl CND");
    int* count=(int*)shmat(CV->shmid,NULL,0);
    if(count==(int*)-1) error("shmat");
    *count=0;
    shmdt(count);
}

void cond_lock(Cond_t* CV)
{
    P(CV->semid,MTX);
}

void cond_unlock(Cond_t* CV)
{
    V(CV->semid,MTX);
}

void cond_wait(Cond_t* CV)
{
    int* count=(int*)shmat(CV->shmid,NULL,0);
    if(count==(int*)-1) error("shmat");
    ++(*count);
    cond_unlock(CV);
    sleep(1);
    P(CV->semid,CND);
    cond_lock(CV);
    --(*count);
    shmdt(count);
}

void cond_signal(Cond_t* CV)
{
    int* count=(int*)shmat(CV->shmid,NULL,0);
    if(count==(int*)-1) error("shmat");
    if(*count>0)
        V(CV->semid,CND);
    shmdt(count);
}

void cond_broadcast(Cond_t* CV)
{
    int* count=(int*)shmat(CV->shmid,NULL,0);
    if(count==(int*)-1) error("shmat");
    if((*count)>0)
    {
        for(int i=0;i<*count;i++)
            V(CV->semid,CND);
    }
    shmdt(count);
}

void cond_destroy(Cond_t* CV)
{
    semctl(CV->semid,0,IPC_RMID);
    shmctl(CV->shmid,IPC_RMID,NULL);
}
