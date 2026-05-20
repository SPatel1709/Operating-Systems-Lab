#include "global.hpp"
#include "manager.cpp"


int main(int argc,char* argv[]){

    if(argc!=5)
    {
        std::cerr<<"(ERROR) Usage: ./foobar <numFooCars> <numBarCars> <fooFile> <barFile>\n";
        exit(EXIT_FAILURE);
    }

    fooCars=atoi(argv[1]);
    barCars=atoi(argv[2]);
    fooSchedFile=static_cast<std::string_view>(argv[3]);
    barSchedFile=static_cast<std::string_view>(argv[4]);

    pthread_t mainThread;
    pthread_create(&mainThread,NULL,&main_thread,NULL);
    pthread_join(mainThread,NULL);
    return 0;
}