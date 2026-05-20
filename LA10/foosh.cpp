#include "diskutils.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// namespace {

std::vector<std::string> split_args(const std::string &line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string token;
    while (ss >> token) {
        out.push_back(token);
    }
    return out;
}

std::string prompt_from_cwd(const std::string &cwd) {
    if (cwd == "/") {
        return "[foosh] VD:> ";
    }
    return "[foosh] VD:" + cwd + "> ";
}
// }

int main() {
    if (joindisk() != 0) {
        std::cerr << "*** Error: unable to join disk run diskmanager create first\n";
        return 1;
    }

    unsigned int current_block = RBN;
    std::string cwd = "/";

    std::cout << "+++ Number of blocks = " << NBLOCKS << "\n";
    std::cout << "+++ Number of free blocks = " << NFREEBLOCKS << "\n";
    std::cout << "+++ First block of the root directory = " << RBN << "\n";

    std::string line;
    while (true) {
        std::cout << prompt_from_cwd(cwd);
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }

        const auto args = split_args(line);
        if (args.empty()) {
            continue;
        }

        const std::string &cmd = args[0];

        if (cmd == "exit" || cmd == "quit") {
            break;
        }

        if (cmd == "cd" || cmd == "chdir") {
            if (args.size() == 1U) {
                current_block = RBN;
                cwd = "/";
                continue;
            }
            metadata target{};
            char absbuf[1024] = {0};
            if (vfs_resolve(current_block, cwd.c_str(), args[1].c_str(), &target, absbuf, sizeof(absbuf)) != 0 || target.type != 'd') {
                std::cout << "*** Error: unable to change to directory " << args[1] << "\n";
                continue;
            }
            current_block = target.firstblock;
            cwd = absbuf;
            if (cwd.empty()) {
                cwd = "/";
            }
            continue;
        }

        if (cmd == "md" || cmd == "mkdir") {
            if (args.size() != 2U) {
                std::cout << "*** Error: usage md path\n";
                continue;
            }
            vfs_mkdir(current_block, cwd.c_str(), args[1].c_str());
            continue;
        }

        if (cmd == "dir") {
            vfs_dir(current_block);
            continue;
        }

        if (cmd == "ls") {
            if (args.size() == 1U) {
                vfs_ls(current_block, cwd.c_str(), nullptr);
            } else {
                vfs_ls(current_block, cwd.c_str(), args[1].c_str());
            }
            continue;
        }

        if (cmd == "prn" || cmd == "type") {
            if (args.size() != 2U) {
                std::cout << "*** Error: usage prn file\n";
                continue;
            }
            vfs_prn(current_block, cwd.c_str(), args[1].c_str());
            continue;
        }

        if (cmd == "cp" || cmd == "copy") {
            if (args.size() != 3U) {
                std::cout << "*** Error: usage cp src dst\n";
                continue;
            }
            vfs_cp(current_block, cwd.c_str(), args[1].c_str(), args[2].c_str());
            continue;
        }

        std::cout << "*** Error: unknown command " << cmd << "\n";
    }

    std::cout << "+++ Number of free blocks = " << NFREEBLOCKS << "\n";
    leavedisk();
    return 0;
}
