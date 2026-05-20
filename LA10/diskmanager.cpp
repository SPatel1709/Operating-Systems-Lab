#include "diskutils.h"

#include <sys/ipc.h>
#include <sys/shm.h>

#include <cstring>
#include <iostream>

namespace {

unsigned int block_offset(unsigned int block_no) {
    return block_no * BLOCK_SIZE;
}

void write_u32(unsigned char *disk, unsigned int off, unsigned int value) {
    std::memcpy(disk + off, &value, sizeof(unsigned int));
}

void write_fat(unsigned char *disk, unsigned int block_no, unsigned int next_block) {
    const unsigned int off = FAT_START_BLOCK * BLOCK_SIZE + block_no * 4U;
    write_u32(disk, off, next_block);
}

void bitmap_set_used(unsigned char *disk, unsigned int block_no, bool used) {
    const unsigned int byte_index = block_no / 8U;
    const unsigned int bit_index = block_no % 8U;
    const unsigned int off = BITMAP_START_BLOCK * BLOCK_SIZE + byte_index;
    if (used) {
        disk[off] = static_cast<unsigned char>(disk[off] | (1U << bit_index));
    } else {
        disk[off] = static_cast<unsigned char>(disk[off] & ~(1U << bit_index));
    }
}

void init_root(unsigned char *disk) {
    metadata dot{};
    dot.type = 'd';
    std::strncpy(dot.name, ".", sizeof(dot.name) - 1U);
    dot.size = 2;
    dot.firstblock = ROOT_BLOCK;

    metadata dotdot{};
    dotdot.type = 'd';
    std::strncpy(dotdot.name, "..", sizeof(dotdot.name) - 1U);
    dotdot.size = 2;
    dotdot.firstblock = ROOT_BLOCK;

    const unsigned int off = block_offset(ROOT_BLOCK);
    std::memset(disk + off, 0, BLOCK_SIZE);
    std::memcpy(disk + off, &dot, sizeof(metadata));
    std::memcpy(disk + off + sizeof(metadata), &dotdot, sizeof(metadata));
}

void create_disk(unsigned char *disk) {
    std::memset(disk, 0, NBLOCKS_DEFAULT * BLOCK_SIZE);

    write_u32(disk, 0, NBLOCKS_DEFAULT);
    write_u32(disk, 4, NBLOCKS_DEFAULT - DATA_START_BLOCK);
    write_u32(disk, 8, ROOT_BLOCK);

    for (unsigned int block = 0; block < DATA_START_BLOCK; ++block) {
        bitmap_set_used(disk, block, true);
    }

    for (unsigned int block = 0; block < NBLOCKS_DEFAULT; ++block) {
        write_fat(disk, block, 0);
    }

    init_root(disk);
}

// helper code
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " create|remove\n";
        return 1;
    }

    const key_t key = ftok(DISK_SHM_FTOK_PATH, DISK_SHM_FTOK_PROJ);
    if (key == -1) {
        std::perror("ftok");
        return 1;
    }

    const std::string mode = argv[1];

    if (mode == "create") {
        const int shmid = shmget(key, NBLOCKS_DEFAULT * BLOCK_SIZE, IPC_CREAT | 0666);
        if (shmid == -1) {
            std::perror("shmget");
            return 1;
        }
        void *addr = shmat(shmid, nullptr, 0);
        if (addr == reinterpret_cast<void *>(-1)) {
            std::perror("shmat");
            return 1;
        }

        create_disk(static_cast<unsigned char *>(addr));

        if (shmdt(addr) == -1) {
            std::perror("shmdt");
            return 1;
        }

        std::cout << "virtual disk created and initialized\n";
        return 0;
    }

    if (mode == "remove") {
        const int shmid = shmget(key, NBLOCKS_DEFAULT * BLOCK_SIZE, 0666);
        if (shmid == -1) {
            std::perror("shmget");
            return 1;
        }
        if (shmctl(shmid, IPC_RMID, nullptr) == -1) {
            std::perror("shmctl");
            return 1;
        }
        std::cout << "virtual disk removed\n";
        return 0;
    }

    std::cerr << "invalid mode use create or remove\n";
    return 1;
}
