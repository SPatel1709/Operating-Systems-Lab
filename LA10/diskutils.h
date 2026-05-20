#ifndef DISKUTILS_H
#define DISKUTILS_H

#include <cstddef>

#define BLOCK_SIZE 1024U
#define NBLOCKS_DEFAULT 65536U
#define BITMAP_START_BLOCK 1U
#define BITMAP_BLOCK_COUNT 8U
#define FAT_START_BLOCK 9U
#define FAT_BLOCK_COUNT 256U
#define ROOT_BLOCK 265U
#define DATA_START_BLOCK 266U
#define METADATA_NAME_MAX 22U
#define DISK_SHM_FTOK_PATH "/tmp"
#define DISK_SHM_FTOK_PROJ 'F'

typedef struct metadata {
    char type;
    char name[23];
    unsigned int size;
    unsigned int firstblock;
} metadata;

extern unsigned char *D;
extern unsigned int NBLOCKS;
extern unsigned int NFREEBLOCKS;
extern unsigned int RBN;

int joindisk(void);
void leavedisk(void);
unsigned int getfreeblock(void);
void freeblock(unsigned int block_no);

// abs means absolute path ;) cool naming style
int vfs_mkdir(unsigned int cwd_block, const char *cwd_abs, const char *path);
int vfs_ls(unsigned int cwd_block, const char *cwd_abs, const char *path);
int vfs_dir(unsigned int cwd_block);
int vfs_prn(unsigned int cwd_block, const char *cwd_abs, const char *path);
int vfs_cp(unsigned int cwd_block, const char *cwd_abs, const char *src, const char *dst);

int vfs_resolve(unsigned int cwd_block, const char *cwd_abs, const char *path, metadata *out_meta, char *out_abs, size_t out_abs_size);

#endif
