#include <iostream>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fstream>
#include <cerrno>
#include <cstring>
#include <string>

const int BOARDSIZE = 101;
const std::string fileName = "ludo.txt";
const key_t boardKey = 130;
const key_t playerKey = 140;

int* boardMem = nullptr;
int* playerMem = nullptr;
int boardShmid = -1;
int playerShmid = -1;

void populate_board();
void populate_player_pos(int players);
void initialize_shared_memory(int players);
void cleanup(pid_t xbpPid, pid_t xppPid, pid_t bpPid, pid_t ppPid);
void spawn_processes(int players, int pipefd[], pid_t* xbpPid, pid_t* xppPid, pid_t* bpPid, pid_t* ppPid);
void game_loop(int players, int pipefd, pid_t ppPid);

void populate_board() {
    for(int pos = 0; pos <= 100; ++pos) {
        boardMem[pos] = 0;
    }
    
    std::ifstream file(fileName);
    
    if(!file) {
        std::cerr << "No Ludo.txt found!!\n";
        exit(1);
    }
    
    char entity;
    while(file >> entity) {
        if(entity == 'E') break;
        
        int start, end;
        file >> start >> end;
        
        int difference = end - start;
        boardMem[start] = difference;
    }
    
    file.close();
}

void populate_player_pos(int players) {
    for(int index = 0; index < players; ++index) {
        playerMem[index] = 0;
    }
    
    playerMem[players] = players;
}

void initialize_shared_memory(int players) {
    // Create board shared memory
    boardShmid = shmget(boardKey, BOARDSIZE * sizeof(int), IPC_CREAT | 0666);
    if(boardShmid == -1) {
        std::cerr << "Failed to create board shared memory\n";
        exit(1);
    }
    
    boardMem = (int*)shmat(boardShmid, nullptr, 0);
    if(boardMem == (int*)(-1)) {
        std::cerr << "Failed to attach board shared memory\n";
        exit(1);
    }
    
    populate_board();
    
    // Create player shared memory
    playerShmid = shmget(playerKey, (players + 1) * sizeof(int), IPC_CREAT | 0666);
    if(playerShmid == -1) {
        std::cerr << "Failed to create player shared memory\n";
        exit(1);
    }
    
    playerMem = (int*)shmat(playerShmid, nullptr, 0);
    if(playerMem == (int*)(-1)) {
        std::cerr << "Failed to attach player shared memory\n";
        exit(1);
    }
    
    populate_player_pos(players);
}

void spawn_processes(int players, int pipefd[], pid_t* xbpPid, pid_t* xppPid, pid_t* bpPid, pid_t* ppPid) {
    char playersArg[10], pipeArg[10], bpArg[10];
    snprintf(playersArg, 10, "%d", players);
    snprintf(pipeArg, 10, "%d", pipefd[1]);
    
    if((*xbpPid = fork()) == 0) {
        close(pipefd[0]);
        execlp("xterm", "xterm", "-T", "Board", "-fa", "Monospace", "-fs", "10", 
               "-geometry", "100x24+100+100", "-bg", "#000033",
               "-e", "./board", playersArg, pipeArg, NULL);
        exit(1);
    }
    
    read(pipefd[0], bpPid, sizeof(pid_t));
    snprintf(bpArg, 10, "%d", *bpPid);
    
    if((*xppPid = fork()) == 0) {
        close(pipefd[0]);
        execlp("xterm", "xterm", "-T", "Players", "-fa", "Monospace", "-fs", "8",
               "-geometry", "60x24+1300+100", "-bg", "#003309",
               "-e", "./players", playersArg, pipeArg, bpArg, NULL);
        exit(1);
    }
    
    read(pipefd[0], ppPid, sizeof(pid_t));
    
    char ack;
    read(pipefd[0], &ack, 1);
}

void game_loop(int players, int pipefd, pid_t ppPid) {
    std::cout << "Game started. Enter Commands:\n";
    std::cout << "next - make next move\n";
    std::cout << "autoplay <delay_ms> - autoplay mode\n";
    std::cout << " quit - end game\n";
    
    bool autoplay = false;
    int delay = 1000;
    
    while(true) {
        std::string command;
        
        if(!autoplay) {
            std::cout << "$ ";
            std::cin >> command;
        } else {
            usleep(delay * 1000);
            command = "next";
        }
        
        if(command == "next") {
            kill(ppPid, SIGUSR1);
            char ack;
            read(pipefd, &ack, 1);
            
            if(playerMem[players] == 0) {
                std::cout << "\nGame Over! All players finished\n";
                sleep(1);
                break;
            }
        } else if(command == "autoplay") {
            if(!autoplay) {
                std::cin >> delay;
                autoplay = true;
                std::cout << "Autoplay mode ON ( delay: " << delay << " ms)\n";
            }
        } else if(command == "quit") {
            std::cout << "\nQuitting game...\n";
            break;
        }
    }
}

void cleanup(pid_t xbpPid, pid_t xppPid, pid_t bpPid, pid_t ppPid) {
    kill(ppPid, SIGUSR2);
    waitpid(xppPid, NULL, 0);
    std::cout << "Players exited...\n";
    
    kill(bpPid, SIGINT);
    waitpid(xbpPid, NULL, 0);
    std::cout << "Board disconnected...\n";
    
    shmdt(boardMem);
    shmdt(playerMem);
    shmctl(boardShmid, IPC_RMID, 0);
    shmctl(playerShmid, IPC_RMID, 0);
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cerr << "Usage: ./ludo_down <numPlayers>\n";
        exit(1);
    }
    
    int players = atoi(argv[1]);
    
    initialize_shared_memory(players);
    
    int pipefd[2];
    if(pipe(pipefd) == -1) {
        std::cerr << "Pipe creation failed\n";
        exit(1);
    }
    
    pid_t xbpPid, xppPid, bpPid, ppPid;
    spawn_processes(players, pipefd, &xbpPid, &xppPid, &bpPid, &ppPid);
    
    game_loop(players, pipefd[0], ppPid);
    
    cleanup(xbpPid, xppPid, bpPid, ppPid);
    close(pipefd[0]);
    
    return 0;
}
