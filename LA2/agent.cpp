#include <iostream>
#include <fstream>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <cassert>
#include <vector>
#include <queue>
#include <algorithm>
#include <random>
#include <cstdlib>
#include <string>
#include <numeric>

using CustomerID=int;
using CustomerQueue=std::queue<CustomerID>;
using ChildPIDTable=std::vector<pid_t>;

CustomerQueue gen_random_queue(CustomerID totalCustomers);  /*generates random queue*/
ChildPIDTable create_child_process(int totalCustomers);     /*creates child process for each customer*/

/*the main ticket allotment logic*/
void ticket_allotment(CustomerQueue &customerQ,ChildPIDTable &pidTable,int tickets,CustomerID totalCustomers);

/*printing queue info*/
void print_info(const CustomerQueue& customerQ,int tickets);

/*reading ticket request from the request.txt*/
int read_ticket_request();

/*terminate any child if left*/
void terminate_child(CustomerQueue &customerQ,ChildPIDTable &ChildPIDTable);

static int g_ticketRequest = 0;
static int g_terminationRequest = 0;

void signal_handler(int sig) {
    if (sig == SIGUSR1)
        g_ticketRequest = 1;
    else if (sig == SIGUSR2)
        g_terminationRequest = 1;
}

int main(int argc,char* argv[]){
    
    assert(argc>=3);
    
    
    int tickets{std::stoi(argv[1])};
    int totalCustomers{std::stoi(argv[2])};
    
    CustomerQueue customerQ{gen_random_queue(totalCustomers)};
    ChildPIDTable table{create_child_process(totalCustomers)};
    
    signal(SIGUSR1,signal_handler);
    signal(SIGUSR2,signal_handler);

    ticket_allotment(customerQ,table, tickets, totalCustomers);
    
    return 0;
}



CustomerQueue gen_random_queue(CustomerID totalCustomers)
{
    std::vector<CustomerID> nums(totalCustomers);
    std::iota(nums.begin(),nums.end(),1);
    
    std::mt19937 rng(getpid());
    std::shuffle(nums.begin(),nums.end(),rng);

    std::queue<CustomerID> q;
    for(int num: nums) {
        q.push(num);
    }
    return q;
}

ChildPIDTable create_child_process(int totalCustomers)
{
    std::vector<pid_t> customerQ;
    customerQ.reserve(totalCustomers+1);
    customerQ.push_back(-1);

    for(int count{1};count<=totalCustomers;++count)
    {
        pid_t pid=fork();
        if(pid<0)
        {
            perror("Failed to create child processes\n");
        }
        else if(pid>0)
        {
            customerQ.push_back(pid);
        }
        else{
            // Child process
            std::string countStr = std::to_string(count);
            execl("./customer","customer", countStr.c_str(), NULL);
            perror("execl failed");
            exit(1);  // Must exit if execl fails!
        }
        sleep(1);
    }

    return customerQ;
}


void ticket_allotment(CustomerQueue &customerQ,ChildPIDTable &pidTable,int tickets,CustomerID totalCustomers)
{
    
    while(!customerQ.empty() && tickets>0)
    {
        print_info(customerQ,tickets);
        CustomerID customer{customerQ.front()};
        customerQ.pop();

        kill(pidTable[customer],SIGUSR1);

        g_ticketRequest=0;
        g_terminationRequest=0;
        while(!g_ticketRequest && !g_terminationRequest)
        {
            pause();
        }

        if(g_terminationRequest) {
            waitpid(pidTable[customer],NULL,0);
            continue;
        }

        int ticketRequest{read_ticket_request()};
        if(ticketRequest<=tickets)
        {
            kill(pidTable[customer],SIGUSR1);
            tickets=tickets-ticketRequest;
        }
        else{
            kill(pidTable[customer],SIGUSR2);
        }
        sleep(1);

        customerQ.push(customer);        
    }

    if(!customerQ.empty())
    terminate_child(customerQ,pidTable);
    else{
        std::cout<<"Agent: Booking session over (no more customers available)\n";
    }
}

void print_info(const CustomerQueue& customerQ,int tickets)
{
    CustomerQueue tempQueue = customerQ; 
    std::cout<<"Agent: Queue ( ";
    while(!tempQueue.empty())
    {
        std::cout<<tempQueue.front()<<" ";
        tempQueue.pop();
    }
    std::cout<<") Available = "<<tickets<<"\n\n";
}

int read_ticket_request()
{
    std::ifstream file("request.txt");
    
    if(!file.is_open())
    {
        std::cerr << "Failed to open request.txt\n";
        return -1;
    }

    int customerID, ticketRequest;
    char colon;
    
    // Read format: "customerID : ticketRequest"
    if(file >> customerID >> colon >> ticketRequest)
    {
        return ticketRequest;
    }
    return -1;
}

void terminate_child(CustomerQueue &customerQ,ChildPIDTable &ChildPIDTable)
{
    std::cout<<"Agent terminates customers ";
    while(!customerQ.empty())
    {
        std::cout<<customerQ.front()<<" ";
        kill(ChildPIDTable[customerQ.front()],SIGKILL);
        customerQ.pop();
    }

    std::cout<<"Agent: Booking session over (no more tickets available)\n";
    
}
