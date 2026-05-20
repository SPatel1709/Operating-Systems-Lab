#include "global.hpp"
#include "work.cpp"
#include <fstream>

void readFile(carType car,std::string_view fileName,int &numWorkers,
        int &numParts,std::vector<std::vector<int>> &toDoList,std::vector<std::vector<int>> &carPreq,std::vector<std::vector<int>> &carDep)
{
    std::fstream file(std::string(fileName),std::ios::in);
    file>>numParts>>numWorkers;

    carPreq.resize(numParts);
    carDep.resize(numParts);

    if(car==carType::CAR_TYPE_FOO)
    std::cout<<"+++ Foocar"<<"\n\n";

    else if(car==carType::CAR_TYPE_BAR)
    std::cout<<"+++ Barcar"<<"\n\n";

    std::cout<<"\tDependencies\n";
    for(int idx=0;idx<numParts;++idx)
    {

        int part,worker;
        file>>part>>worker;

        std::cout<<"\t"<<part<<" -> ";
        toDoList[worker].push_back(part);
        int u=0;
        while(1)
        {
            file>>u;
            if(u==-1) break;

            carDep[part].push_back(u);
            carPreq[u].push_back(part);
            std::cout<<u<<" ";
        }
        std::cout<<"\n";
    }
    file.close();


    std::cout<<"\n\tPrerequisites\n";
    int part=0;
    for(auto &parts:carPreq){
        std::cout<<"\t"<<part<<" <- ";
        ++part;
        for(auto &dep:parts)
        {
            std::cout<<dep<<" ";
        }
        std::cout<<"\n";
    }

    std::cout<<"\n\tWorker Assignment\n";
    int worker=0;



    // this is crazi stl function that works as for loop but as I wanted to use range based
    // approach from begin to numWorkers I could not do for(auto &x:toDoList)
    // as this will work for all the MAX_WORKERS though I loved this new approach 
    // cpp full of cool coding techs
    std::for_each(toDoList.begin(),toDoList.begin()+numWorkers,[&worker](auto &workerList)
        {
            std::cout<<"\t"<<worker<<" : ";
            ++worker;
            for(auto &toDo:workerList)
            {
                std::cout<<toDo<<" ";
            }
            std::cout<<"\n";
        });
    std::cout<<"\n\n";
}


inline void log_failure(std::string_view msg)
{
    std::cerr<<"(ERROR) "<<msg<<std::endl;
    exit(EXIT_FAILURE);
}

void init_pthread_vars(int numWorkers){

    workerPrinting.resize(numWorkers,"");
    // do this first as threads can run and attempt to wait at barrier
    if(pthread_mutex_init(&mutexParts,NULL)!=0){
        log_failure("manager.cpp: init_pthread_vars(): pthread_mutex_init failed");
    }

    if(pthread_barrier_init(&bop,NULL,numWorkers+1)!=0){
        log_failure("manager.cpp: init_pthread_vars(): pthread_barrier_init failed");
    }
    if(pthread_barrier_init(&eop,NULL,numWorkers+1)!=0){
        log_failure("manager.cpp: init_pthread_vars(): pthread_barrier_init failed");
    }

    for(int workers{0};workers<numWorkers;++workers)
    {
        if(pthread_cond_init(&condWait[workers],NULL)!=0){
            log_failure("manager.cpp: init_pthread_vars(): pthread_cond_init failed");
        }
    }

    for(int workers{0};workers<numWorkers;++workers)
    {
        int* id=new int(workers);
        if(pthread_create(&workerThread[workers],NULL,&Worker,(void*)id)!=0){
            log_failure("manager.cpp: init_pthread_vars(): pthread_create failed"); 
        }
    }
}


void join_pthread_workers(int numWorkers)
{
    for(int worker=0;worker<numWorkers;++worker)
    {
        if(pthread_join(workerThread[worker],NULL)!=0)
        {
            std::cerr<<"(ERROR) manager.cpp: join_pthread_workers(): pthread_join_failed\n";
        }
    }
}

void destroy_pthread_vars(int numWorker){
    for(int workers{0};workers<numWorker;++workers)
    {
        pthread_cond_destroy(&condWait[workers]);
    }

    pthread_mutex_destroy(&mutexParts);
    pthread_barrier_destroy(&bop);
    pthread_barrier_destroy(&eop);
}

void switch_car_production(){
    if(carTypeInProduction==carType::CAR_TYPE_BAR)
        {
            --barCars;
            if(fooCars>0)
            {
                carTypeInProduction=carType::CAR_TYPE_FOO;
            }
        }
        else if(carTypeInProduction==carType::CAR_TYPE_FOO)
        {
            --fooCars;
            if(barCars>0)
            {
                carTypeInProduction=carType::CAR_TYPE_BAR;
            }
    }

    if(fooCars+barCars==0)
    {
        carTypeInProduction=carType::CAR_TYPE_NONE;
    }
}


void* main_thread(void* args)
{
    readFile(carType::CAR_TYPE_FOO,fooSchedFile,fooWorkers,fooParts,fooToDoList,fooPreq,fooDep);
    readFile(carType::CAR_TYPE_BAR,barSchedFile,barWorkers,barParts,barToDoList,barPreq,barDep);
    
    int maxWorkers=std::max(fooWorkers,barWorkers);
    
    if(fooCars+barCars<=0)
    {
        std::cout<<"No cars in production\n";
    }
    else{
        
        if(fooCars>0) carTypeInProduction=carType::CAR_TYPE_FOO;
        else carTypeInProduction=carType::CAR_TYPE_BAR;
        
        init_pthread_vars(maxWorkers);
    
        while(1)
        {
            std::fill(PSTAT.begin(),PSTAT.end(),partStat::PART_STAT_PENDING);
            std::fill(WINFO.begin(),WINFO.end(),-1);
            print_car_in_production(maxWorkers);
            pthread_barrier_wait(&bop);

            if(fooCars==0 && barCars==0)
            break; // this is valid as I am ensuring that all workers stop at barrier bop

        
            pthread_barrier_wait(&eop);
            std::cout<<"\n\n";
            switch_car_production();
        }
        
        join_pthread_workers(maxWorkers);
        destroy_pthread_vars(maxWorkers);
    }
    return nullptr;
}