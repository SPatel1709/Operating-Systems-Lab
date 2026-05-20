#include <iostream>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iomanip>


static int g_usr1Handler=0;
static int g_usr2Handler=0;

constexpr int MIN_TICKETS=1;
constexpr int MAX_TICKETS=4;
constexpr int MIN_BOOKINGS=1;
constexpr int MAX_BOOKINGS=2;


void signal_handler(int sig);
void booking_attempt(int customerID);
void make_request(int customerID,int ticketReq);


void signal_handler(int sig)
{
    if(sig==SIGUSR1) g_usr1Handler=1; // Do write to request.txt operation or successful booking request
    if(sig==SIGUSR2) g_usr2Handler=1; //  Booking failed status
}

int main(int argc,char* argv[]){

    assert(argc==2);

    srand(getpid());  // Seed random number generator once
    
    int customerID{std::stoi(argv[1])};
    std::cout<<"\t\t\t\tCustomer "<<std::setw(4)<<customerID<<" joins the booking system \n";

    signal(SIGUSR1,signal_handler);
    signal(SIGUSR2,signal_handler);

    booking_attempt(customerID);

    return 0;
}


void booking_attempt(int customerID){

    int bookingNumber{1};
    while(1)
    {
        g_usr1Handler=0;
        while(!g_usr1Handler) pause();

        int wantToBook{rand()%MAX_BOOKINGS};
        if(bookingNumber>MAX_BOOKINGS /*Will not book the third time*/||  (bookingNumber>MIN_BOOKINGS && !wantToBook)/*0 means dont want to buy*/){
            kill(getppid(),SIGUSR2);
            std::cout<<"\t\t\t\tCustomer "<<std::setw(4)<<customerID<<" leaves the booking system\n";
            break;
        }
        else
        {
            int ticket{rand()%MAX_TICKETS+MIN_TICKETS};
            make_request(customerID,ticket);
            kill(getppid(),SIGUSR1);
        }

        g_usr1Handler=0;
        g_usr2Handler=0;
        while(!g_usr1Handler && !g_usr2Handler) pause();

        if(g_usr1Handler) {
            std::cout<<"\t\t\t\tCustomer "<<std::setw(4)<<customerID<<" : Booking "<<bookingNumber<<" successful\n\n";
            ++bookingNumber;
        }
        
        if(g_usr2Handler) std::cout<<"\t\t\t\tCustomer "<<std::setw(4)<<customerID<<" : Booking "<<bookingNumber<<" failed\n\n";
    }
}



void make_request(int customerID,int ticketReq){

        std::cout<<"\t\t\t\tCustomer "<<std::setw(4)<<customerID<<" : Request for "<<ticketReq<<" tickets\n";
    
        std::ofstream file("request.txt");
        if(file.is_open()) {
            file << customerID << " : " << ticketReq << std::endl;
            file.flush();
        }
        else{
            std::cerr<<"Unable to open the file\n";
        }
}
