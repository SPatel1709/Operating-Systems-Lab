
#ifndef LOGGER_H
#define LOGGER_H
#include "global.hpp"


// made just for outputing in formatted manner.


void print_car_in_production(int workers){
    if(carTypeInProduction==carType::CAR_TYPE_BAR)
    {
        std::cout<<"+++ Production of a barcar begins\n\n    ";
    }
    else if(carTypeInProduction==carType::CAR_TYPE_FOO){                                                 
        std::cout<<"+++ Production of a foocar begins\n\n    ";
    }
    else{
        std::cout<<"+++ All productions completed\n\n    ";
    }

    for(int i=0;i<workers;++i)
    {
        std::cout<<std::setw(13)<<("WORKER "+std::to_string(i));
    }
    std::cout<<"\n    ";
    for(int i=0;i<workers;++i)
    {
        std::cout<<std::setw(13)<<"--------";
    }
    std::cout<<std::endl;
}

void assign_worker_string(int workerId,std::string_view msg,bool newRow=true)
{
    if(newRow)
    std::fill(workerPrinting.begin(), workerPrinting.end(), "");

    workerPrinting[workerId]=msg;
}

void print_worker(){
    std::cout<<"    ";
    for(auto &x:workerPrinting)
    {
        if (x.empty())
            std::cout<<std::setw(13)<< " ";
        else
            std::cout<<std::setw(13)<<x;
    }

    std::cout<<std::endl;
}

#endif