#include <iostream>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <iomanip>
#include <cstring>
#include <sys/types.h>
#include <fstream>
#include <string>

const int playerKey = 140;
std::string snakeLadder[10];
int* playerMem = nullptr;
int numPlayers = 0;
int pipeFd;

void attach_player_memory();
void print_players_at(int position);
void print_hori_line();
void print_board();
void sigusr1_handler(int sig);

void get_snake_ladder() {
    std::ifstream file("ludo.txt");
    
    if(!file) {
        std::cerr << "Not able to access ludo.txt\n";
        return;
    }
    
    char tok;
    while(file >> tok) {
        if(tok == 'E') break;
        
        int start, end;
        file >> start >> end;
        
        std::string temp = "\t" + std::string(1, tok) + "(" + std::to_string(start) + " -> " + std::to_string(end) + ")";
        
        if(start % 10 == 0)
            snakeLadder[start/10 - 1] += temp;
        else
            snakeLadder[start/10] += temp;
    }
    
    for(int i = 9; i >= 0; --i) {
        std::cout << snakeLadder[i] << std::endl;
    }
    file.close();
}

bool is_player_at(int position, int& playerNum) {
    if(playerMem == nullptr || playerMem == (int*)(-1))
        return false;
    
    for(int i = 0; i < numPlayers; ++i) {
        if(playerMem[i] == position) {
            playerNum = i;
            return true;
        }
    }
    return false;
}

void print_players_at(int position) {
    for(int i = 0; i < numPlayers; ++i) {
        if(playerMem && playerMem[i] == position)
            std::cout << (char)(i + 'A') << ' ';
    }
    std::cout << '\n';
}

void print_hori_line() {
    for(int i = 0; i < 10; ++i)
        std::cout << "+----";
    std::cout << "+\n";
}

void print_board() {
    std::cout << "\n\n\n\n";
    print_players_at(100);
    
    for(int pos = 100; pos > 0; pos -= 10) {
        print_hori_line();
        
        for(int j = pos; j > pos - 10; --j) {
            int playerNum;
            if(j != 100 && is_player_at(j, playerNum)) {
                std::cout << '|' << std::left << std::setw(4) << (char)(playerNum + 'A') << std::right;
            } else {
                std::cout << '|' << std::setw(4) << j;
            }
        }
        std::cout << "| " << snakeLadder[pos/10 - 1] << std::endl;
    }
    
    print_hori_line();
    print_players_at(0);
}

void sigusr1_handler(int sig) {
    print_board();
    write(pipeFd, "A", 1);
}

void attach_player_memory() {
    int shmid = shmget(playerKey, numPlayers * sizeof(int), 0444);
    if(shmid == -1) {
        std::cerr << "Failed to get shared memory\n";
        exit(1);
    }
    
    playerMem = (int*)shmat(shmid, nullptr, SHM_RDONLY);
    if(playerMem == (int*)(-1)) {
        std::cerr << "Failed to attach shared memory\n";
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    if(argc < 3) {
        std::cerr << "Usage: ./board_down players pipeFd\n";
        exit(1);
    }
    
    numPlayers = atoi(argv[1]);
    pipeFd = atoi(argv[2]);
    
    attach_player_memory();
    
    pid_t myPid = getpid();
    write(pipeFd, &myPid, sizeof(pid_t));
    sleep(1);
    
    signal(SIGUSR1, sigusr1_handler);
    
    get_snake_ladder();
    sigusr1_handler(0);
    write(pipeFd, "A", 1);
    
    while(1) {
        pause();
    }
    
    shmdt(playerMem);
    return 0;
}
