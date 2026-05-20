#include <iostream>
#include <string>
#include <memory>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sstream>

using namespace std;

// I am using unique ptrs here instead of raw pointers for better memory management

int g_alphabetSize;  // alphabet size
int g_numStates;     // number of states
unique_ptr<unique_ptr<int[]>[]> g_delta;  // transition function
unique_ptr<int[]> g_isFinal;  // final state markers
unique_ptr<unique_ptr<int[]>[]> g_pipes;  // pipes for all states

// File descriptors for original stdin and stdout
int g_originalStdin;
int g_originalStdout;

// marco commands
#define CMD_TRANSITION 1
#define CMD_QUIT 2
#define END_OF_INPUT '$'


void read_dfa(const string& filename);  /* read the dfa from the file */
void coordinator_loop();
void state_loop(int stateNum);
void signal_handler(int signum);
void change_descriptor(int fd, int new_fd);

// Helper function to change file descriptor
void change_descriptor(int fd, int new_fd) {
    close(fd);
    dup(new_fd);
}

// Signal handler for coordinator
void signal_handler(int signum) {
    #ifdef _VERBOSE
    cout << "\n\n\t\t\t  +++ Coordinator going to terminate all state processes" << endl;
    #endif
    
    // QUIT command
    for (int i = 0; i < g_numStates; i++) {
        change_descriptor(STDOUT_FILENO, g_pipes[i + 1][1]);
        fflush(stdout);
        cout << CMD_QUIT << endl;
        fflush(stdout);
    }
    
    // wait for all state processes to terminate
    for (int i = 0; i < g_numStates; i++) {
        wait(NULL);
    }
    
    // Restore stdout
    change_descriptor(STDOUT_FILENO, g_originalStdout);
    fflush(stdout);
    
    #ifdef _VERBOSE
    cout << "\t\t\t\t  +++ Coordinator: Bye" << endl;
    #else
    cout << "\n\t\t\t+++ Coordinator: All state processes terminated. Bye." << endl;
    #endif
    
    exit(0);
}

void read_dfa(const string& filename) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        cerr << "Error: Cannot open file " << filename << endl;
        exit(1);
    }
    
    char buffer[4096];
    // ssize_t is the signed version of the size_t datatype.
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    if (bytesRead == -1) {
        cerr << "Error: Cannot read file " << filename << endl;
        close(fd);
        exit(1);
    }
    buffer[bytesRead] = '\0';
    
    stringstream ss(buffer);    // to parse the buffer quickly
    
    ss >> g_alphabetSize;
    ss >> g_numStates;
    
    g_delta = make_unique<unique_ptr<int[]>[]>(g_numStates);
    g_isFinal = make_unique<int[]>(g_numStates);
    
    for (int i = 0; i < g_numStates; i++) {
        g_delta[i] = make_unique<int[]>(g_alphabetSize);
        int state;
        char finalChar;
        ss >> state >> finalChar;
        g_isFinal[state] = (finalChar == 'F') ? 1 : 0;
        
        for (int j = 0; j < g_alphabetSize; j++) {
            ss >> g_delta[state][j];
        }
    }
    
    close(fd);
}

void state_loop(int stateNum) {

    // receive transition information from coordinator
    change_descriptor(STDIN_FILENO, g_pipes[stateNum + 1][0]);
    fflush(stdin);
    
    int finalStatus;
    cin >> finalStatus;
    
    auto transitions = make_unique<int[]>(g_alphabetSize);
    for (int i = 0; i < g_alphabetSize; i++) {
        cin >> transitions[i];
    }
    
    
    while (true) {
        change_descriptor(STDIN_FILENO, g_pipes[stateNum + 1][0]);
        
        int command;
        cin >> command;        cin.ignore(); // newline issues to be handles with ingore
        
        if (command == CMD_QUIT) {
            #ifdef _VERBOSE
            change_descriptor(STDOUT_FILENO, g_originalStdout);
            fflush(stdout);
            cout << "\t\t\t\t  +++ State " << stateNum << " going to quit" << endl;
            fflush(stdout);
            #endif
            exit(0);
        }
        else if (command == CMD_TRANSITION) {

            change_descriptor(STDOUT_FILENO, g_pipes[0][1]);
            fflush(stdout);
            cout << stateNum << endl;
            fflush(stdout);
            
            change_descriptor(STDIN_FILENO, g_pipes[stateNum + 1][0]);
            
            char symbol;
            cin.get(symbol);   cin.ignore();  // ignore the newline after symbol
            
            if (symbol == END_OF_INPUT) {
                // end of input reached
                change_descriptor(STDOUT_FILENO, g_originalStdout);
                fflush(stdout);
                if (finalStatus) {
                    cout << " ACCEPT" << endl;
                } else {
                    cout << " REJECT" << endl;
                }
                fflush(stdout);
                
                // send completion signal back to coordinator
                change_descriptor(STDOUT_FILENO, g_pipes[0][1]);
                fflush(stdout);
                cout << "DONE" << endl;
                fflush(stdout);
            }
            else {
                // check if symbol is valid
                int symbolIndex = symbol - 'a';

                if (symbolIndex < 0 || symbolIndex >= g_alphabetSize) {
                    change_descriptor(STDOUT_FILENO, g_originalStdout);
                    fflush(stdout);
                    cout << " INVALID INPUT SYMBOL: " << symbol << endl;
                    fflush(stdout);
                    
                    change_descriptor(STDOUT_FILENO, g_pipes[0][1]);   fflush(stdout);
                    cout << "DONE" << endl;
                    fflush(stdout);
                }
                else {
                    // perform transition sybol valid ehre
                    int nextState = transitions[symbolIndex];
                    
                    change_descriptor(STDOUT_FILENO, g_originalStdout);
                    fflush(stdout);
                    cout << " -- " << symbol << " --> " << nextState;
                    fflush(stdout);
                    
                    // send TRANSITION command to next state
                    change_descriptor(STDOUT_FILENO, g_pipes[nextState + 1][1]);
                    fflush(stdout);
                    cout << CMD_TRANSITION << endl;
                    fflush(stdout);
                }
            }
        }
    }
}

// coordinator loop function
void coordinator_loop() {
    #ifdef _VERBOSE
    cout << "\t\t\t\t+++ Coordinator: Going to user loop" << endl;
    #endif
    
    while (true) {
        // Restore stdin and stdout to terminal
        change_descriptor(STDIN_FILENO, g_originalStdin);
        change_descriptor(STDOUT_FILENO, g_originalStdout);
        fflush(stdout);
        
        cout << "Enter next string: ";
        fflush(stdout);
        
        string input;
        if (!getline(cin, input)) {
            break;
        }
        
        change_descriptor(STDOUT_FILENO, g_originalStdout);
        cout << "0";
        change_descriptor(STDOUT_FILENO, g_pipes[1][1]);
        cout << CMD_TRANSITION << endl;
        bool invalid = false;
        for (size_t i = 0; i < input.length(); i++) {
            change_descriptor(STDIN_FILENO, g_pipes[0][0]);
            int currentState; 
            cin >> currentState; 
            cin.ignore();
            change_descriptor(STDOUT_FILENO, g_pipes[currentState + 1][1]);
            cout << input[i] << '\n';
            int symbolIndex = input[i] - 'a';
            if (symbolIndex < 0 || symbolIndex >= g_alphabetSize) { 
                invalid = true; 
                break; 
            }
        }
        change_descriptor(STDIN_FILENO, g_pipes[0][0]);
        int currentState; 
        string done;
        if (!invalid) {
            cin >> currentState; 
            cin.ignore();
            change_descriptor(STDOUT_FILENO, g_pipes[currentState + 1][1]);
            cout << END_OF_INPUT << '\n';
            change_descriptor(STDIN_FILENO, g_pipes[0][0]);
            cin >> done; 
            cin.ignore();
        } else {
            cin >> done; 
            cin.ignore();
        }
    }
}

int main(int argc, char* argv[]) {
    // Determine DFA file name
    string filename = (argc > 1) ? string(argv[1]) : "dfa.txt";
    
    read_dfa(filename);
    
    // Save original stdin and stdout
    g_originalStdin = dup(STDIN_FILENO);
    g_originalStdout = dup(STDOUT_FILENO);
    
    // Create n+1 pipes (one for coordinator, n for state processes)
    g_pipes = make_unique<unique_ptr<int[]>[]>(g_numStates + 1);
    for (int i = 0; i <= g_numStates; i++) {
        g_pipes[i] = make_unique<int[]>(2);
        if (pipe(g_pipes[i].get()) == -1) {
            cerr << "Error creating pipe" << endl;
            exit(1);
        }
    }
    
    // Fork n state processes
    auto childPids = make_unique<pid_t[]>(g_numStates);
    for (int i = 0; i < g_numStates; i++) {
        childPids[i] = fork();
        if (childPids[i] == -1) {
            cerr << "Error forking process" << endl;
            exit(1);
        }
        else if (childPids[i] == 0) {
            // Child process (state process)
            // Ignore SIGINT
            signal(SIGINT, SIG_IGN);
            // Enter state loop
            state_loop(i);
            exit(0);  // Should never reach here
        }
    }
    
    // Parent process (coordinator)
    // Register SIGINT handler
    signal(SIGINT, signal_handler);
    
    // Wait for state processes to initialize
    sleep(1);
    
    // Send initial information to each state process
    for (int i = 0; i < g_numStates; i++) {
        change_descriptor(STDOUT_FILENO, g_pipes[i + 1][1]);
        fflush(stdout);
        
        cout << g_isFinal[i] << endl;
        for (int j = 0; j < g_alphabetSize; j++) {
            cout << g_delta[i][j];
            if (j < g_alphabetSize - 1) cout << " ";
        }
        cout << endl;
        fflush(stdout);
        
        #ifdef _VERBOSE
        change_descriptor(STDOUT_FILENO, g_originalStdout);
        fflush(stdout);
        if (g_isFinal[i]) {
            cout << "\t\t\t\t\t+++ Final state " << i << " created" << endl;
        } else {
            cout << "\t\t\t\t+++ Non-final state " << i << " created" << endl;
        }
        fflush(stdout);
        #endif
    }
    
    // Restore stdout
    change_descriptor(STDOUT_FILENO, g_originalStdout);
    fflush(stdout);
    
    cout << "\t\t\t+++ Coordinator: " << g_numStates << " state processes are created" << endl;
    
    // Enter coordinator loop
    coordinator_loop();
    
    return 0;
}