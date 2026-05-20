#include <iostream>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <cstdlib>
#include <ctime>

const int boardKey = 130;
const int playerKey = 140;
const int BOARDSIZE = 101;

pid_t boardPid = 0;
pid_t playerPid[100];
int pipeFd = 0;
int g_numPlayers = 0;
int currentPlayer = 0;

int* boardMem = nullptr;
int* playerMem = nullptr;
int playerIndex = 0;

int check_occupied(int pos);
void update_move(int sum);
int roll_dice();
void player_run();
void xpp_sigusr1_handler(int sig);
void xpp_sigusr2_handler(int sig);
void pp_sigusr1_handler(int sig);
void attach_shared_memory();
void fork_players();

int check_occupied(int pos) {
    if(pos != 100) {
        for(int i = 0; i < g_numPlayers; ++i) {
            if(i != playerIndex && playerMem[i] == pos) {
                return i;
            }
        }
    }
    return -1;
}


void update_move(int sum) {
    int currentPos = playerMem[playerIndex];
    int nextPos = currentPos + sum;
    bool playerFinished = false;
    
    if(nextPos > 100) {
        std::cout << "Move not permitted (cannot go beyond 100)\n";
    } else {
        while(boardMem[nextPos] != 0) {
            int change = boardMem[nextPos];
            std::cout << (change > 0 ? "Ladder" : "Snake")
                      << " at cell " << nextPos
                      << ", Jump to " << (nextPos + change) << std::endl;
            nextPos += change;
        }
        
        int playerOcc = check_occupied(nextPos);
        if(playerOcc != -1) {
            std::cout << "Move not permitted (cell occupied by "
                      << (char)('A' + playerOcc) << ")\n";
        } else {
            std::cout << "Player " << (char)('A' + playerIndex)
                      << " moves to cell " << nextPos << std::endl;
            playerMem[playerIndex] = nextPos;
            
            if(nextPos == 100) {
                playerMem[g_numPlayers]--;
                playerFinished = true;
            }
        }
    }
    
    std::cout << "***********************************************************\n";
    kill(boardPid, SIGUSR1);
    
    if(playerFinished)
        exit(0);
}

int roll_dice() {
    int currentPos = playerMem[playerIndex];
    
    if(currentPos == 100) return -1;
    
    int rolls = 0;
    int sum = 0;
    
    std::cout << "Player " << (char)('A' + playerIndex) << ": ";
    while(1) {
        int turn = rand() % 6 + 1;
        sum += turn;
        
        std::cout << ((rolls) ? ("+ ") : ("")) << turn << " ";
        
        if(turn != 6) break;
        
        ++rolls;
        if(rolls == 3) {
            std::cout << " X ";
            sum = 0;
            rolls = 0;
        }
    }
    std::cout << std::endl;
    
    return sum;
}

void player_run() {
    while(1) {
        pause();
    }
}


void xpp_sigusr1_handler(int sig) {
    for(int count = 0; count < g_numPlayers; ++count) {
        if(playerMem[currentPlayer] < 100) {
            kill(playerPid[currentPlayer], SIGUSR1);
            currentPlayer = (currentPlayer + 1) % g_numPlayers;
            return;
        }
        currentPlayer = (currentPlayer + 1) % g_numPlayers;
    }
}

void xpp_sigusr2_handler(int sig) {
    for(int i = 0; i < g_numPlayers; ++i) {
        if(playerMem[i] < 100) {
            kill(playerPid[i], SIGINT);
        }
    }
    
    for(int i = 0; i < g_numPlayers; ++i) {
        waitpid(playerPid[i], NULL, 0);
    }
    exit(0);
}


void pp_sigusr1_handler(int sig)
{
    int diceSum=roll_dice();
    if(diceSum!=-1)
    update_move(diceSum);
}

void attach_shared_memory() {
    int boardShmid = shmget(boardKey, BOARDSIZE * sizeof(int), 0444);
    if(boardShmid == -1) {
        std::cerr << "Failed to get board shared memory\n";
        exit(1);
    }
    
    boardMem = (int*)shmat(boardShmid, nullptr, SHM_RDONLY);
    if(boardMem == (int*)(-1)) {
        std::cerr << "Failed to attach board shared memory\n";
        exit(1);
    }
    
    int playerShmid = shmget(playerKey, (g_numPlayers + 1) * sizeof(int), 0666);
    if(playerShmid == -1) {
        std::cerr << "Failed to get player shared memory\n";
        exit(1);
    }
    
    playerMem = (int*)shmat(playerShmid, nullptr, 0);
    if(playerMem == (int*)(-1)) {
        std::cerr << "Failed to attach player shared memory\n";
        exit(1);
    }
}

void fork_players() {
    srand(time(NULL));
    
    for(int i = 0; i < g_numPlayers; ++i) {
        pid_t pid = fork();
        
        if(pid == 0) {
            signal(SIGUSR1, pp_sigusr1_handler);
            signal(SIGUSR2, SIG_DFL);
            
            srand(time(NULL) + i);
            playerIndex = i;
            player_run();
        }
        playerPid[i] = pid;
    }
}

int main(int argc, char* argv[]) {
    if(argc < 4) {
        std::cerr << "Usage: ./players_down numPlayers pipeFd BoardPID\n";
        exit(1);
    }
    
    g_numPlayers = atoi(argv[1]);
    pipeFd = atoi(argv[2]);
    boardPid = atoi(argv[3]);
    
    pid_t mypid = getpid();
    write(pipeFd, &mypid, sizeof(pid_t));
    
    attach_shared_memory();
    
    signal(SIGUSR1, xpp_sigusr1_handler);
    signal(SIGUSR2, xpp_sigusr2_handler);
    
    fork_players();
    
    while(1) {
        pause();
    }
    
    shmdt(boardMem);
    shmdt(playerMem);
    return 0;
}
