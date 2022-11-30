#include "disk_emu.h"
#include "sfs_api.h"

SuperBlock superBlockCache; // in-memory cache for the super block
iNodesTable iNodesTableCache; // in-memory cache for the i-Node table
Block freeBlockListCache; // in-memory cache for the free bitmap/blocklist
OpenFileDescriptorTable openFDTCache; // in-memory cache for the open file descriptor table
struct { // in-memory cache for all the root directory entries/files
    DirectoryEntry directoryEntries[TOTAL_FILES];
    int location; // pointer to the location of a file on device (mentioned in textbook pg 530)
} rootDirectoryCache;


int allocateBlock()
{
    for (int blockIndex = 0; blockIndex < DISK_BLOCK_SIZE; blockIndex++)
    {
        if (freeBlockListCache.data[blockIndex] == FreeBlock) { // there is a free block available for use
            freeBlockListCache.data[blockIndex] = OccupiedBlock; // mark block as occupied
            write_blocks(FreeBlockListIndex, 1, &freeBlockListCache); // update the disk free block list information
            return blockIndex;
        }
    }
    printf("ERROR in allocateBlock: no free blocks available in the free block list.");
    return -1;
}

void mksfs(int fresh)
{
    char *diskName = "disko";

    if (fresh) {
        // Initializing a fresh disk in the emulator
        init_fresh_disk(diskName, DISK_BLOCK_SIZE, DISK_DATA_BLOCKS);

        // Before allocating any blocks, need to initialize the free blocks list (free bit map) in the emulator
        freeBlockListCache.data[FreeBlockListIndex] = OccupiedBlock; // index of the free block list location so it will always be occupied
        for (int i = 0; i < DISK_BLOCK_SIZE-1; i++)
        {
            freeBlockListCache.data[i] = FreeBlock;
        }
        write_blocks(FreeBlockListIndex, 1, &freeBlockListCache); // saving the free block list to the disk emulator

        // Initializing the root directory pointed to by the super block
        iNode rootDirectory; // note: a directory (root directory or any other) is still a type i-Node
        for (int i = 0; i < DIRECT_POINTERS; i++)
        {
            rootDirectory.directPointers[i] = allocateBlock(); // find a block to store the root directory
        }
        rootDirectory.size = INITIALIZATION_VALUE; // this field will change when the root directory is filled up
        rootDirectory.indirectPointer = INITIALIZATION_VALUE;

        // Initializing the in-memory super block and saving it to the disk (on-disk super block)
        superBlockCache.magic = MAGIC;
        superBlockCache.blockSize = DISK_BLOCK_SIZE;
        superBlockCache.fileSystemSize = DISK_DATA_BLOCKS; // since super block and root dir are part of the total disk data blocks we don't add them
        superBlockCache.iNodeTableLength = 12;//DISK_DATA_BLOCKS;
        superBlockCache.rootDirectory = rootDirectory;
        strcpy(superBlockCache.name, "Super Block");
        write_blocks(SuperBlockIndex, 1, &superBlockCache); // saving the super block on the disk emulator

        // Initializing the in-memory root directory and saving it to the disk (on-disk root directory)
        for (int i = 0; i < TOTAL_FILES; i++)
        {
            rootDirectoryCache.directoryEntries[i].filename[0] = EMPTY_STRING;
        }

        for (int i = 0; i < TOTAL_ROOT_DIRECTORY_BLOCKS; i++)
        {
            allocateBlock();
        }
        rootDirectoryCache.location = START_INDEX;
        write_blocks(RootDirectoryIndex, 1, &rootDirectoryCache);

        // Initializing the in-memory i-Node table and saving it to the disk (on-disk i-Node table)
        for (int i = 0; i < TOTAL_FILES; i++)
        {
            iNodesTableCache.iNodes[i].mode = -1; // del
            iNodesTableCache.iNodes[i].linkCount = INITIALIZATION_VALUE;
            iNodesTableCache.iNodes[i].uid = -1; // del
            iNodesTableCache.iNodes[i].gid = -1; // del
            iNodesTableCache.iNodes[i].size = INITIALIZATION_VALUE;

            for (int j = 0; j < DIRECT_POINTERS; j++)
            {
                iNodesTableCache.iNodes[i].directPointers[j] = INITIALIZATION_VALUE;
            }
            iNodesTableCache.iNodes[i].indirectPointer = INITIALIZATION_VALUE;
        }
        void *buffer = (void*) malloc(DISK_BLOCK_SIZE * DIRECT_POINTERS); // CHANGE
        memcpy(buffer, &iNodesTableCache, DISK_BLOCK_SIZE * DIRECT_POINTERS); // CHANGE
        write_blocks(iNodeTableIndex, DIRECT_POINTERS, buffer); // CHANGE
        free(buffer); // CHANGE
    }
    else {
        // Opening an existing disk and read its data
        init_disk(diskName, DISK_BLOCK_SIZE, DISK_DATA_BLOCKS);
        read_blocks(SuperBlockIndex, 1, &superBlockCache);
        read_blocks(iNodeTableIndex, DIRECT_POINTERS, &iNodesTableCache);
        read_blocks(RootDirectoryIndex, 1, &rootDirectoryCache);
        read_blocks(FreeBlockListIndex, 1, &freeBlockListCache);
    }

    // Initialize the open file descriptor table
    for (int i = 0; i < TOTAL_FILES; i++)
    {
        openFDTCache.readPointers[i] = INITIALIZATION_VALUE;
        openFDTCache.writePointers[i] = INITIALIZATION_VALUE;
    }
}

int sfs_fopen(char* fname)
{
    return 1;
}

int sfs_fread(int fd, char* buf, int count)
{
    return 1;
}

int sfs_fseek(int fd, int location)
{
    return 0;
}

int sfs_fwrite(int fd, const char* buf, int count)
{
    return 0;
}

int sfs_fclose(int fd)
{
    return 0;
}

int sfs_remove(char* fname)
{
    return 0;
}

int sfs_getnextfilename(char* fname)
{
    return 1;
}

int sfs_getfilesize(const char* path)
{
    return 0;
}