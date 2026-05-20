#include "global.hpp"
#include "logger.hpp"

bool check_prereq(int part)
{
    auto &preqRef = (carTypeInProduction == carType::CAR_TYPE_FOO) ? fooPreq : barPreq;

    bool allDone = true;
    for (auto &prereq : preqRef[part])
    {
        if (PSTAT[prereq] == partStat::PART_STAT_PENDING)
        {
            allDone = false;
            break;
        }
    }
    return allDone;
}

int get_waiting_worker(int part)
{
    for (size_t i = 0; i < MAX_WORKERS; ++i)
    {
        if (part == WINFO[i])
        {
            return i;
        }
    }
    return -1;
}


void *Worker(void *args)
{

    int workerId = *static_cast<int *>(args);
    delete (int *)args;
    while (1)
    {
        WSTAT[workerId]=workStat::WORK_STAT_START; // this does not need to be done with mutex as this will wait till all others are done.
        pthread_barrier_wait(&bop);

        // Main and important logic for loop breaking
        if(fooCars==0 && barCars==0) break;

        // auto &preqRef=(carType==carType::CAR_TYPE_FOO)?fooPreq:barPreq;
        auto isFoo = (carTypeInProduction == carType::CAR_TYPE_FOO);
        auto &toDoListRef=isFoo?fooToDoList:barToDoList;
        auto &carDep=isFoo?fooDep:barDep;

        for (auto &part : toDoListRef[workerId])
        {
            pthread_mutex_lock(&mutexParts);
            bool allDone = check_prereq(part);
            if (allDone == false)
            {
                // std::cout << "Worker " << workerId << " Wait " << part << std::endl;
                assign_worker_string(workerId,"Wait "+std::to_string(part),1);
                print_worker();
                WINFO[workerId] = part;
                WSTAT[workerId] = workStat::WORK_STAT_WAITING;
                pthread_cond_wait(&condWait[workerId], &mutexParts);
            }
            WSTAT[workerId] = workStat::WORK_STAT_WORKING;
            
            PSTAT[part] = partStat::PART_STAT_DONE; 
            // std::cout << "Worker " << workerId << " Part " << part << std::endl;
            assign_worker_string(workerId,"Part "+std::to_string(part),1);
            
            for (auto &dep : carDep[part])
            {
                bool depDone = check_prereq(dep);
                
                if (depDone)
                {
                    int waitingWorker = get_waiting_worker(dep);
                    if(waitingWorker!=-1 && WSTAT[waitingWorker]==workStat::WORK_STAT_WAITING)
                    {
                        pthread_cond_signal(&condWait[waitingWorker]);
                        // std::cout << " Signaled worker " << waitingWorker << std::endl;
                        assign_worker_string(waitingWorker,"Wake up",0);
                    }
                }
            }
            print_worker();
            pthread_mutex_unlock(&mutexParts);
        }
        pthread_mutex_lock(&mutexParts);
        WSTAT[workerId] = workStat::WORK_STAT_DONE;
        // std::cout << "Worker " << workerId << " All Done" << std::endl;
        assign_worker_string(workerId,"All done",1);
        print_worker();
        pthread_mutex_unlock(&mutexParts);
        pthread_barrier_wait(&eop);
    }

    pthread_mutex_lock(&mutexParts);
    WINFO[workerId] = -1;
    // std::cout << "Worker " << workerId << " quit\n";
    assign_worker_string(workerId,"Quit",1);
    print_worker();
    pthread_mutex_unlock(&mutexParts);
    return nullptr;
}