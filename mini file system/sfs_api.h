/**
 * @author Zhanna Klimanova (zhanna.klimanova@mail.mcgill.ca)
 * @brief
 * @version disko
 * @date 2022-12-05
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef SFS_API_H
#define SFS_API_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "disk_emu.h"


#define DIRECT_POINTERS 12
#define INDIRECT_POINTERS DISK_BLOCK_SIZE/sizeof(int)
#define MAX_FILENAME_LENGTH 35 // characters
#define MAGIC 0xACBD0005 // way to identify the format of the file that is holding the emulated disk partition
#define DISK_BLOCK_SIZE 1024 // byte block size in the disk (note: the larger the block size, the greater the internal fragmentation)
#define DISK_DATA_BLOCKS 2000 // directory size 2000
#define TOTAL_FILES 300 // number of files/directories
#define INITIALIZATION_VALUE -1 // struct field initialization values
#define FDT_INITIALIZER_VALUE -1
#define TOTAL_ROOT_DIRECTORY_BLOCKS 6 // 1
#define EMPTY_STRING '\0'
#define START_INDEX 0

enum BlockUtilizationState {FreeBlock = 1, OccupiedBlock = 0};
enum DiskDataStructureIndices {
    SuperBlockIndex = 0,
    iNodeTableIndex = 1,
    RootDirectoryIndex = 301,
    FreeBlockListIndex = 0x000003FF // DISK_BLOCK_SIZE-1
};
enum ReturnErrorCodes {
    fOpenError = -1,
    fCloseError = -1,
    fWriteError = -1,
    fReadError = -1,
    fSeekError = -1,
    fRemoveError = -1,
    getnextfilenameError = -1,
    getfilesizeError = -1,
    allocateBlockError = -1,
    NoError = 0
};

/**
 * @brief block type for the disk emulator (the disk is an array of blocks of fixed size).
 *
 */
typedef struct Block_t { // block is composed of 1024 bytes
    char data[DISK_BLOCK_SIZE]; // all the individual bytes are stored in the data array
} Block;

/**
 * @brief the singly indirect block pointer points to a block of pointers, each of which points to a data block.
 *
 */
typedef struct IndirectBlock_t {
    int blockOfPointers[INDIRECT_POINTERS];
} IndirectBlock;

/**
 * @brief the file or directory in the Simple File System (SFS) is defined by an i-Node. In the case of this SFS,
 *        there is a single root directory (no subdirectories). This root directory is pointed to by an i-Node, which
 *        is pointed to by the super block. The i-Node structure is also simplified: it does not have the double and triple
 *        indirect pointers; instead, it has direct and indirect pointers.
 */
typedef struct iNode_t {
    int linkCount; // i-Node availability: linkCount = 0 when i-Node is unused; linkCount = 1 when i-Node is used
    int size; // everytime something is written to file, size field is changed
    // A pointer is 4 bytes
    int directPointers[DIRECT_POINTERS];
    int indirectPointer;
} iNode;

/**
 * @brief the super block defines the file system geometry; it is the first block in the Simple File System (SFS).
 */
typedef struct SuperBlock_t {
    char name[sizeof("Super Block")];
    int magic; // indicates the type of data in the file
    int blockSize;
    int fileSystemSize; // number of blocks
    int iNodeTableLength; // number of blocks
    iNode rootDirectory; // i-Node pointing to the root directory is stored in the super block
    // The rest is unused space
} SuperBlock;

/**
 * @brief i-Node table is included as part of the on-disk data structures and is also
 *        brought into in-memory.
 *
 */
typedef struct iNodesTable_t {
    char name[sizeof("i-Node Table")];
    iNode iNodes[TOTAL_FILES];
} iNodesTable;

/**
 * @brief the root directory has directory entries which are all files in the case of the
 *        Simple File System; each file has a filename and the file's unique identifier (id). The
 *        identifier is a unique tag, usually a number, and identifies the file within the
 *        file system; it is the non-human readable name for the file.
 *
 */
typedef struct DirectoryEntry_t { // directory entry is composed of 20 bytes
    char filename[MAX_FILENAME_LENGTH];
    // int id; // i-Node associated with the file
    // iNode fileINode; // i-Node associated with the file
} DirectoryEntry;

/**
 * @brief when a file is opened, an entry is created in the File Descriptor Table (same as the Open File Descriptor Table)
 *        in the Simple File System (SFS).
 *
 */
typedef struct OpenFileDescriptorTable_t {
    int read_writePointers[TOTAL_FILES];
} OpenFileDescriptorTable;

/**
 * @brief loops over the free block list (free bitmap) and locates any available
 *        blocks. An available block is marked with 1 while an occupied block is marked
 *        with 0.
 *
 * @return int
 */
int allocateBlock();

/**
 * @brief formats the virtual disk implemented by the disk emulator
 *        and creates an instance of the simple file system on top of it.
 *        mksfs also sets up the in-memory data structures.
 *
 * @param fresh if the fresh flag is enabled (1), the file system should be created
 *              from scratch; else, the file system is opened from the disk (assuming
 *              a valid file system is already in the disk).
 */
void mksfs(int fresh);

/**
 * @brief copies the name of the next file in the directory into the fname
 *        and returns non zero if there is a new file. Once all files have
 *        been returned, this function returns 0.
 *
 * @param fname
 * @return int
 */
int sfs_getnextfilename(char* fname);

/**
 * @brief obtains the size (in bytes) of the given file in the root directory.
 *
 * @param path
 * @return int
 */
int sfs_getfilesize(const char* path);

/**
 * @brief opens a file and returns the index that corresponds to the newly opened
 *        file in the file descriptor table. If the file does not exist, it creates
 *        a new file and sets its size to 0. If the file exists, the file is opened
 *        in append mode (e.g, set the file pointer to the end of the line).
 *
 * @param fname
 * @return int file descriptor that is returned when a file is opened and an entry is created
 *             in the file descriptor table.
 */
int sfs_fopen(char* fname);

/**
 * @brief closes the file pointed to by the file descriptor and removes the entry from
 *        the per-process and system file descriptor tables. The file still persists in the
 *        root directory, is represented by an i-Node, and when opened again will be referenced
 *        in the file descriptor table.
 *
 * @param fd
 * @return int
 */
int sfs_fclose(int fd);

/**
 * @brief moves the read/write pointer to the given location and returns 0 on success.
 *
 * @param fd
 * @param location
 * @return int
 */
int sfs_fseek(int fd, int location);

/**
 * @brief reads from the file into the buffer, starting from the current file pointer, and
 *        returns the number of bytes read.
 *
 * @param fd
 * @param buf
 * @param count
 * @return int
 */
int sfs_fread(int fd, char* buf, int count);

/**
 * @brief writes the given number of bytes of data in buffer into the open file, starting
 *        from the current file pointer, and returns the number of bytes written.
 *
 * @param fd
 * @param buf
 * @param count
 * @return int
 */
int sfs_fwrite(int fd, const char* buf, int count);

/**
 * @brief removes the file from the directory entry, releases the i-Node and releases the
 *        data blocks used by the file (i.e., the data blocks are added to the free block list)
 *        so that they can be used by new files in the future.
 *
 * @param fname
 * @return int
 */
int sfs_remove(char *fname);

#endif
