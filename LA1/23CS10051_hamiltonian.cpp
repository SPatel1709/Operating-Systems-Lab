// Assignment 1
// Shrey Patel
// 23CS10051


// USE CPP 20 OR MORE
// gcc gengraph.c -o gengraph && ./gengraph 
// g++ -std=c++20 23CS10051_hamiltonian.cpp -o hamiltonian && ./hamiltonian



#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <vector>      // vectors
#include <algorithm>   // std::find function
#include <cstdlib>  
#include <cstring>
#include <fstream>
#include <sstream>

using Graph=std::vector<std::vector<int>>;
using ExitStatus=bool;


Graph readGraph(const std::string&,int&);                                               /* Reads graph from the gengraph.c file */
std::vector<int> getCurrentPath(int argc, char* argv[]);                                /* Gets the current path from the argv */
std::vector<bool> getVisited(std::vector<int>& path,std::size_t);                       /* returns the visted vector from the path traversed till now */
ExitStatus hamPath(std::vector<std::vector<int>>& adj,std::vector<int>& path,char*);    /* adds neighbour node for the current node and generates child process again*/
ExitStatus checkCycle(Graph& adj,std::vector<int> &path);                               /* Checks if the lastnode is connected to the firstnode of the path*/



int main(int argc, char* argv[]){

    int n{};
    /*Type alias*/ Graph adj = readGraph("graph.txt", n);

    std::vector<int> path{getCurrentPath(argc, argv)};


    if(path.empty())
    {
        pid_t pid=fork();

        if(pid<0)
        {
            std::cerr<<"Fork failed!"<<std::endl;
            exit(1);
        }
        else if(pid==0)
        {
            std::vector<char*> args;
            args.push_back(argv[0]);
            args.push_back((char*)"1");
            args.push_back(nullptr);

            execv(argv[0],args.data());

            std::cerr<<"Exec failed!"<<std::endl;
            exit(1);
        }
        else
        {
            int status{};
            wait(&status);

            if(WIFEXITED(status) && WEXITSTATUS(status)==0)
            {
                exit(0);
            }
            else {
                std::cout<<"\n\nNo Hamiltonian Cycle found.\n";
                exit(1);
            }
        }
    }
    
    
    else if(path.size()==n)
    {
        ExitStatus findCycle{checkCycle(adj,path)};

        if(findCycle) exit(0);
        else exit(1);
    }

    //not visited all the nodes
    ExitStatus foundCycle{hamPath(adj,path,argv[0])};


    if(foundCycle) exit(0);
    else exit(1);
}



Graph readGraph(const std::string& fileName, int& n) {
    std::ifstream file(fileName);
    file >> n;
    
    std::vector<std::vector<int>> adj(n + 1);
    std::string line;
    getline(file, line); // consume the newline after n
    
    for (int i = 1; i <= n; ++i) {
        getline(file, line);
        std::stringstream ss(line);
        int vertex;
        std::string arrow;
        ss >> vertex >> arrow; // read "vertex ->"
        
        int neighbor;
        while (ss >> neighbor) {
            adj[vertex].push_back(neighbor);
        }
    }
    
    return adj;
}


std::vector<int> getCurrentPath(int argc, char* argv[]) {
    std::vector<int> path;
    for (int i = 1; i < argc; ++i) {
        path.push_back(atoi(argv[i]));
    }

    std::cout << "*** Process " << getpid() << ":";
    for(int v : path) {
        std::cout << " " << v;
    }
    std::cout << std::endl;

    return path;
}


std::vector<bool> getVisited(std::vector<int> &path,std::size_t vertices)
{
    std::vector<bool> visited(vertices,false);

    for(auto &node:path) visited[node]=true;
    return visited;
}


ExitStatus hamPath(Graph &adj,std::vector<int>& path,char* exeName)
{
    std::vector<bool> isVisited{getVisited(path,adj.size())};

    int currentNode{path.back()};
    bool foundCycle{false};

    for(auto &neighbour:adj[currentNode])
    {
        if(!isVisited[neighbour])
        {
            pid_t pid=fork();

            if(pid<0)
            {
                std::cerr<<"Failed to do fork.\n";
                exit(1);
            }
            else if(pid==0)
            {
                std::vector<char*> args;

                args.push_back(exeName);

                path.push_back(neighbour);

                std::vector<std::string> pathStrs;
                for(auto &node:path) {
                    pathStrs.push_back(std::to_string(node));
                }
                for(auto &s : pathStrs) {
                    args.push_back(const_cast<char*>(s.c_str()));
                }

                args.push_back(nullptr);
                execv(/*fileName*/ args[0],args.data());

                std::cerr<<"Exec failed to run.\n";
                exit(1);
            }
            else{
                int status{};
                wait(&status);

                if(WIFEXITED(status) and WEXITSTATUS(status)==0)
                {
                    foundCycle=true;
                    break;
                }
            }
        }
    }

    return foundCycle;

}

ExitStatus checkCycle(Graph& adj,std::vector<int> &path)
{
        int lastNode=path.back();
        int startNode=path.front();
        bool isCycle=std::find(adj[lastNode].begin(),adj[lastNode].end(),startNode)!=adj[lastNode].end();
        
        if(isCycle)
        {
            std::cout<<"\n\nHamiltonian Cycle found:\n";
            for(auto &node:path) std::cout<<node<<" ";
            std::cout<<startNode<<'\n';
            return true;
        }
        else{
            return false;
        }
}


 

