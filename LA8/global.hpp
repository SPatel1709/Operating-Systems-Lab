#pragma once

#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include <queue>
using namespace std;

typedef enum class ReqType{
    ALLOCATE,
    RELEASE,
    QUIT,
    UNKNOWN
}ReqType_t;

typedef enum class Status{
    ACTIVE,
    EXITED
}Status_t;


// here extern is used as this will say the linker that this variables are declared somewhere else so no need to give space for this vars just
// they are defined here they have declaration somewhere this is one definition rule.
extern int N, M, R, REQFROM, NACTIVE;
extern ReqType_t REQTYPE;

extern vector<Status_t> STATUS;

extern pthread_mutex_t SMTX, RMTX, AMTX;
extern vector<pthread_mutex_t> WMTX;

extern pthread_cond_t SCND, RCND, ACND;
extern vector<pthread_cond_t> WCND;

extern vector<int> AVAILABLE, TOTAL;
extern vector<vector<int>> ALLOCATION, REQUEST, RELEASE;

extern vector<int> RQ;

void check_RQ();
void release_all(int worker);
void print_available();
void print_waiting();

inline void cond_signal_helper(pthread_mutex_t &mtx,pthread_cond_t& cond)
{
        pthread_mutex_lock(&mtx);
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mtx);
}
