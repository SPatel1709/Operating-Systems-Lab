#ifndef GLOBAL_H
#define GLOBAL_H
#include <iostream>
#include <pthread.h>
#include <algorithm>
#include <vector>
#include <memory>
#include <array>
#include <iomanip>

static constexpr int MAX_WORKERS=100;
static constexpr int MAX_PARTS=100;

typedef enum class workStat{
    WORK_STAT_START,
    WORK_STAT_WORKING,
    WORK_STAT_WAITING,
    WORK_STAT_DONE
}workStat_t;

typedef enum class partStat{
    PART_STAT_PENDING,
    PART_STAT_DONE
}partStat_t;

typedef enum class carType{
    CAR_TYPE_FOO,
    CAR_TYPE_BAR,
    CAR_TYPE_NONE
}carType_t;

carType_t carTypeInProduction;

std::array<workStat_t,MAX_WORKERS> WSTAT;
std::array<partStat_t,MAX_PARTS> PSTAT;
std::array<int,MAX_PARTS> WINFO{-1};
std::array<std::string, MAX_WORKERS> PRINTBUF;

std::array<pthread_cond_t,MAX_WORKERS> condWait;
std::array<pthread_t,MAX_WORKERS> workerThread;
pthread_mutex_t mutexParts;
pthread_barrier_t bop,eop;


int fooCars{},barCars{};
int fooParts{},barParts{};
int fooWorkers{},barWorkers{};
std::vector<std::vector<int>> fooDep;
std::vector<std::vector<int>> fooPreq;
std::vector<std::vector<int>> barDep;
std::vector<std::vector<int>> barPreq;

std::vector<std::vector<int>> fooToDoList(MAX_WORKERS);
std::vector<std::vector<int>> barToDoList(MAX_WORKERS);

std::vector<std::string> workerPrinting;

// this is the lighter than strings so using this here
std::string_view fooSchedFile;
std::string_view barSchedFile;


#endif