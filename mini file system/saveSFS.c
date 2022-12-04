#include "sfs_api.h"

SuperBlock superBlockCache; // in-memory cache for the super block
iNodesTable iNodesTableCache; // in-memory cache for the i-Node table
Block freeBlockListCache; // in-memory cache for the free bitmap/blocklist
OpenFileDescriptorTable openFDTCache; // in-memory cache for the open file descriptor table
struct { // in-memory cache for all the root directory entries/files
    DirectoryEntry directoryEntries[TOTAL_FILES];
    int location; // pointer to the location of a file on device (mentioned in textbook pg 530)
} rootDirectoryCache;

/**
 * Read data block from disk emulator
 *
 * @param   start_address start address to read from (# of blocks)
 * @return  pointer to buffer with read data
 */
void *read_block(int start_address) {
    int blocksToRead = 1;
    void *buffer = (void*) malloc(DISK_BLOCK_SIZE); // Allocate a empty block
    read_blocks(start_address, blocksToRead, buffer);
    return buffer;
}

/**
 * Save Free Bit Map to disk.
 */
void flush_fbm_to_disk() {
    write_blocks(FreeBlockListIndex, 1, &freeBlockListCache);
}

/**
 * Save root directory the disk.
 */
void flush_root_directory_to_disk() {
    write_blocks(RootDirectoryIndex, 4, &rootDirectoryCache);
}

/**
 * Save inode table to the disk.
 */
void flush_inode_table_to_disk() {
    void *buffer = (void*) malloc(DISK_BLOCK_SIZE * DIRECT_POINTERS);
    memcpy(buffer, &iNodesTableCache, DISK_BLOCK_SIZE * DIRECT_POINTERS);
    write_blocks(iNodeTableIndex, DIRECT_POINTERS, buffer);
    free(buffer);
}

/**
 * Find and allocate free data block in disk.
 *
 * @return  free block index
 */
int allocate_new_block() {
    for (int i = 0; i < DISK_BLOCK_SIZE; i++) {  // Loop over bytes
        if (freeBlockListCache.data[i] != 0) {  // Free block found
            freeBlockListCache.data[i] = 0;
            flush_fbm_to_disk();    // Save to disk
            return i;   // Return block index
        }
    }
    return -1;  // ERROR: No free blocks left
}

void mksfs(int fresh) {
    char *diskName = "disko";
    if (fresh) {
        /**************INITLIAZE NEW DISK IN EMULATOR**************/
        init_fresh_disk(diskName, DISK_BLOCK_SIZE, DISK_DATA_BLOCKS);

        /**************INITLIAZE FREE BLOCKS LIST**************/
        // Before allocating any blocks, need to initialize the free blocks list (free bit map) in the emulator
        freeBlockListCache.data[FreeBlockListIndex] = OccupiedBlock;
        for (int i = 0; i < DISK_BLOCK_SIZE-1; i++)
        {
            freeBlockListCache.data[i] = FreeBlock;
        }
        write_blocks(FreeBlockListIndex, 1, &freeBlockListCache); // saving the free block list to the disk emulator

        /**************INITLIAZE SUPER BLOCK**************/
        iNode rootDirectory; // note: a directory (root directory or any other) is still a type i-Node
        for (int i = 0; i < DIRECT_POINTERS; i++)
        {
            rootDirectory.directPointers[i] = allocate_new_block();  // find a block to store the root directory
        }
        rootDirectory.size = INITIALIZATION_VALUE; // this field will change when the root directory is filled up
        rootDirectory.indirectPointer = INITIALIZATION_VALUE;

        // Initializing the in-memory super block and saving it to the disk (on-disk super block)
        superBlockCache.magic = MAGIC;
        superBlockCache.blockSize = DISK_BLOCK_SIZE;
        superBlockCache.fileSystemSize = DISK_DATA_BLOCKS; // since super block and root dir are part of the total disk data blocks we don't add them
        superBlockCache.iNodeTableLength = 12; //DISK_DATA_BLOCKS;
        superBlockCache.rootDirectory = rootDirectory;
        strcpy(superBlockCache.name, "Super Block");
        write_blocks(SuperBlockIndex, 1, &superBlockCache); // saving the super block on the disk emulator

        /**************INITLIAZE ROOT DIRECTORY**************/
        // Initializing the in-memory root directory and saving it to the disk (on-disk root directory)
        for (int fileIndex = 0; fileIndex < TOTAL_FILES; fileIndex++)
        {
            rootDirectoryCache.directoryEntries[fileIndex].filename[0] = '\0';
        }

        for (int rootDirectoryBlock = 0; rootDirectoryBlock < TOTAL_ROOT_DIRECTORY_BLOCKS; rootDirectoryBlock++)
        {
            allocate_new_block();
        }
        rootDirectoryCache.location = START_INDEX;
        write_blocks(RootDirectoryIndex, 4, &rootDirectoryCache);

        /**************INITLIAZE INODE TABLE**************/
        // Initializing the in-memory i-Node table and saving it to the disk (on-disk i-Node table)
        for (int fileIndex = 0; fileIndex < TOTAL_FILES; fileIndex++)
        {
            iNodesTableCache.iNodes[fileIndex].mode = -1; // del
            iNodesTableCache.iNodes[fileIndex].linkCount = INITIALIZATION_VALUE;
            iNodesTableCache.iNodes[fileIndex].uid = -1; // del
            iNodesTableCache.iNodes[fileIndex].gid = -1; // del
            iNodesTableCache.iNodes[fileIndex].size = INITIALIZATION_VALUE;

            for (int directPointerIndex = 0; directPointerIndex < DIRECT_POINTERS; directPointerIndex++)
            {
                iNodesTableCache.iNodes[fileIndex].directPointers[directPointerIndex] = INITIALIZATION_VALUE;
            }
            iNodesTableCache.iNodes[fileIndex].indirectPointer = INITIALIZATION_VALUE;
        }
        void *buffer = (void*) malloc(DISK_BLOCK_SIZE * DIRECT_POINTERS); // CHANGE
        memcpy(buffer, &iNodesTableCache, DISK_BLOCK_SIZE * DIRECT_POINTERS); // CHANGE
        write_blocks(iNodeTableIndex, DIRECT_POINTERS, buffer); // CHANGE
        free(buffer); // CHANGE

    } else {
        /**************INITLIAZE EXISTING DISK IN EMULATOR**************/
        init_disk(diskName, DISK_BLOCK_SIZE, DISK_DATA_BLOCKS);
        read_blocks(SuperBlockIndex, 1, &superBlockCache);
        read_blocks(iNodeTableIndex, DIRECT_POINTERS, &iNodesTableCache);
        read_blocks(RootDirectoryIndex, 4, &rootDirectoryCache);
        read_blocks(FreeBlockListIndex, 1, &freeBlockListCache);
    }
    /**************INITLIAZE OPEN FILE DESCRIPTOR TABLE**************/
    for (int fileIndex = 0; fileIndex < TOTAL_FILES; fileIndex++) {
        openFDTCache.read_writePointers[fileIndex] = FDT_INITIALIZER_VALUE;
    }
}

/**
 * Copies the name of the next file in the directory into fname and returns non zero if there
 * is a new file. Once all the files have been returned, this function returns 0
 *
 * @param fname buffer to copy the name of next file in directory
 * @return      1 on success, 0 otherwise
 */
int sfs_getnextfilename(char* fname) {
    for (int i = rootDirectoryCache.location; i < TOTAL_FILES; i++) {   // Loop over files
        if (rootDirectoryCache.directoryEntries[i].filename[0] != '\0') { // File with non-empty name found
            rootDirectoryCache.location = i + 1; // Move up search location by one where we stopped searching
            flush_root_directory_to_disk(); // Save root dir search location to disk
            strncpy(fname, rootDirectoryCache.directoryEntries[i].filename, MAX_FILENAME_LENGTH);  // Copy needed data into buffer
            return 1;
        }
    }
    rootDirectoryCache.location = 0; // Reset search location to start
    flush_root_directory_to_disk();     // Save root dir search location to disk

    return 0;
}

/**
 * Get the size of the given file in root directory.
 *
 * @param path  path name of wanted file
 * @return      size in bytes of wanted file
 */
int sfs_getfilesize(const char* path) {
    /**************ERROR CHECKING**************/
    if (strlen(path) < 1 || strlen(path) > MAX_FILENAME_LENGTH)
        return -1; // ERROR: invalid path

    /**************FUNCTION**************/
    for (int i = 0; i < TOTAL_FILES; i++) {   // Loop over files
        if (strcmp(rootDirectoryCache.directoryEntries[i].filename, path) == 0) { // File found (matches path)
            int size = iNodesTableCache.iNodes[i].size;  // Get size of inode
            return size;    // Returns size of file
        }
    }

    return -1;  // ERROR: File doesn't exist
}

int sfs_fopen(char *fname) {
    /**************ERROR CHECKING**************/
    int lenName = strlen(fname);
    if (lenName < 1 || lenName > MAX_FILENAME_LENGTH) {
        printf("ERROR in sfs_fopen: invalid filename - exceeds bounds.\n");
        return fOpenError;
    }

    /**************FUNCTION**************/
    // Case 1: open existing file if the root directory contains it
    int fd = 0;
    while (fd < TOTAL_FILES) {
        if (strcmp(rootDirectoryCache.directoryEntries[fd].filename, fname) == 0) {
            openFDTCache.read_writePointers[fd] = iNodesTableCache.iNodes[fd].size;
            return fd;
        }
        ++fd;
    }

    // Case 2: create new file in a free root directory slot
    fd = 0;
    while (fd < TOTAL_FILES) {
        if (rootDirectoryCache.directoryEntries[fd].filename[0] == EMPTY_STRING) {
            openFDTCache.read_writePointers[fd] = 0;
            iNodesTableCache.iNodes[fd].size = 0;
            strncpy(rootDirectoryCache.directoryEntries[fd].filename, fname, MAX_FILENAME_LENGTH);
            flush_inode_table_to_disk();
            write_blocks(RootDirectoryIndex, TOTAL_ROOT_DIRECTORY_BLOCKS, &rootDirectoryCache);
            return fd;
        }
        ++fd;
    }

    printf("ERROR in sfs_fopen: not enough space left to create a new file.\n");
    return fOpenError;
}

int sfs_fclose(int fd) {
    /**************ERROR CHECKING**************/
    if (fd < 0 || fd >= TOTAL_FILES || openFDTCache.read_writePointers[fd] < 0) {
        printf("ERROR in sfs_fclose: invalid file descriptor.\n");
        return fCloseError;
    }

    /**************FUNCTION**************/
    openFDTCache.read_writePointers[fd] = FDT_INITIALIZER_VALUE;

    return NoError;
}

int sfs_fwrite(int fd, const char *buf, int count) {
    /**************ERROR CHECKING**************/
    if (count < 0) {
        printf("ERROR in sfs_fwrite: invalid number of count bytes.\n");
        return fWriteError;
    }

    if (fd < 0 || fd >= TOTAL_FILES || openFDTCache.read_writePointers[fd] < 0) {
        printf("ERROR in sfs_fwrite: invalid file descriptor.\n");
        return fWriteError;
    }

    if (strlen(buf) > DISK_BLOCK_SIZE * DISK_DATA_BLOCKS) {
        printf("ERROR in sfs_fwrite: number of bytes written to disk is out of range.\n");
        return fWriteError;
    }

    /**************FUNCTION**************/
    iNode iNodeOfFile = iNodesTableCache.iNodes[fd];
    int rwPointer = openFDTCache.read_writePointers[fd];
    int fileSize;
    int numBlocksForFile;

    // Update file size and number of blocks required for the file
    int newSize = rwPointer + count;
    if (newSize > iNodeOfFile.size) {
        fileSize = newSize;
        numBlocksForFile = (fileSize / DISK_BLOCK_SIZE) + ((fileSize % DISK_BLOCK_SIZE) != 0); // ceiling of fileSize / DISK_BLOCK_SIZE
    }
    else {
        fileSize = iNodeOfFile.size; // keep the original file size if new size is smaller than current size
        numBlocksForFile = (fileSize / DISK_BLOCK_SIZE) + ((fileSize % DISK_BLOCK_SIZE) != 0);
    }

    // To write to a file, it is first necessary to get existing data from a file and then append the new data bytes.
    char *fileDataBuffer = malloc(numBlocksForFile * DISK_BLOCK_SIZE);
    sfs_fseek(fd, 0);
    sfs_fread(fd, fileDataBuffer, iNodeOfFile.size);
    sfs_fseek(fd, openFDTCache.read_writePointers[fd]); // read_writePointer has not been updated yet, is it is currently at end of old file
    memcpy(fileDataBuffer + rwPointer, buf, count);  // appending the fileDataBuffer with the buf bytes

    // Given the new amount of file data, it is necessary to redestribute it amongst a new amount of blocks.
    IndirectBlock *indirectBlock = NULL;
    int blockIndexInFreeBlockList = 0;
    int indirectBlockAddressPointer = 0;
    int blockIndex = 0;
    while (blockIndex < numBlocksForFile) {
        if (blockIndex < DIRECT_POINTERS) { // will later distribute file information bytes amongst direct i-Node pointers
            blockIndexInFreeBlockList = iNodeOfFile.directPointers[blockIndex];
            if (blockIndexInFreeBlockList < 0) { // unused block
                blockIndexInFreeBlockList = allocate_new_block();
                iNodesTableCache.iNodes[fd].directPointers[blockIndex] = blockIndexInFreeBlockList;
            }
        } else { // will later distribute file information bytes amongst indirect i-Node pointers b/c ran out of direct pointers
            if (iNodeOfFile.indirectPointer < 0) { // Uninitialized indirect block
                indirectBlockAddressPointer = allocate_new_block();
                iNodeOfFile.indirectPointer = indirectBlockAddressPointer;  // Get new indirect block
                iNodesTableCache.iNodes[fd].indirectPointer = indirectBlockAddressPointer;
                IndirectBlock indirect;
                for (int indirectPointerIndex = 0; indirectPointerIndex < INDIRECT_POINTERS; indirectPointerIndex++) {
                    indirect.blockOfPointers[indirectPointerIndex] = -1; // Reset indirect pointers
                }
                blockIndexInFreeBlockList = allocate_new_block();
                indirect.blockOfPointers[0] = blockIndexInFreeBlockList;
                write_blocks(indirectBlockAddressPointer, 1, &indirect);
            } else { // indirect block initialized
                if (indirectBlock == NULL) {
                    indirectBlock = read_block(iNodeOfFile.indirectPointer);
                }
                if (blockIndex - DIRECT_POINTERS >= INDIRECT_POINTERS) {
                    free(fileDataBuffer);
                    free(indirectBlock);
                    printf("ERROR in sfs_fwrite: not enough blocks to complete block allocation request.\n");
                    return fWriteError;
                }
                blockIndexInFreeBlockList = indirectBlock->blockOfPointers[blockIndex - DIRECT_POINTERS];
                if (blockIndexInFreeBlockList < 0) {  // uninitialized indirect block
                    blockIndexInFreeBlockList = allocate_new_block();
                    indirectBlock->blockOfPointers[blockIndex - DIRECT_POINTERS] = blockIndexInFreeBlockList;
                    write_blocks(iNodeOfFile.indirectPointer, 1, indirectBlock);
                }
            }
        }
        if (blockIndexInFreeBlockList < 0 || blockIndexInFreeBlockList >= DISK_DATA_BLOCKS) { // block index outside bounds
            free(indirectBlock);
            free(fileDataBuffer);
            printf("ERROR in sfs_fwrite: not enough free blocks to complete block allocation request.\n");
            return fWriteError;
        }
        write_blocks(blockIndexInFreeBlockList, 1, fileDataBuffer + (blockIndex * DISK_BLOCK_SIZE));
        ++blockIndex;
    }
    free(indirectBlock);
    free(fileDataBuffer);

    openFDTCache.read_writePointers[fd] = fileSize;
    iNodesTableCache.iNodes[fd].size = fileSize;
    flush_inode_table_to_disk();

    return count;
}

int sfs_fread(int fd, char *buf, int count) {
    /**************ERROR CHECKING**************/
    if (count < 0) {
        printf("ERROR in sfs_fread: invalid number of count bytes.\n");
        return fReadError;
    }

    if (fd < 0 || fd >= TOTAL_FILES || openFDTCache.read_writePointers[fd] < 0) {
        printf("ERROR in sfs_fread: invalid file descriptor.\n");
        return fReadError;
    }

    /**************FUNCTION**************/
    IndirectBlock *indirectBlock = NULL;
    int fileSize = iNodesTableCache.iNodes[fd].size;
    int rwPointer = openFDTCache.read_writePointers[fd];
    int bytesToRead = count;
    int numBlocksForFile = 0;
    char *fileDataBuffer;
    char *blockBuffer;
    int blockNumber;
    int blockIndex = 0;

    if (fileSize < rwPointer + count) { // note: rwPointer is pointing to end of file from fopen
        bytesToRead = fileSize - rwPointer;
    }

    numBlocksForFile = (fileSize / DISK_BLOCK_SIZE) + ((fileSize % DISK_BLOCK_SIZE) != 0); // get number of blocks the file is using
    fileDataBuffer = (void*) malloc(numBlocksForFile * DISK_BLOCK_SIZE);
    blockBuffer = (void*) malloc(DISK_BLOCK_SIZE); // buffer to temporarily store each block
    memset(fileDataBuffer, 0, numBlocksForFile * DISK_BLOCK_SIZE);
    memset(blockBuffer, 0, DISK_BLOCK_SIZE);

    while (blockIndex < numBlocksForFile) {
        if (blockIndex < DIRECT_POINTERS) {
            blockNumber = iNodesTableCache.iNodes[fd].directPointers[blockIndex];
        } else {
            if (indirectBlock == NULL) {
                indirectBlock = read_block(iNodesTableCache.iNodes[fd].indirectPointer);
            }
            if (blockIndex - DIRECT_POINTERS >= INDIRECT_POINTERS) {
                free(indirectBlock);
                free(fileDataBuffer);
                free(blockBuffer);

                printf("ERROR in sfs_fread: not enough blocks to complete block allocation request.\n");
                return fReadError;
            }
            blockNumber = indirectBlock->blockOfPointers[blockIndex - DIRECT_POINTERS];
        }
        if (blockNumber < 0) {
            free(indirectBlock);
            free(fileDataBuffer);
            free(blockBuffer);

            printf("ERROR in sfs_fread: an invalid block number was requested.\n");
            return fReadError;
        }
        read_blocks(blockNumber, 1, blockBuffer);
        memcpy(fileDataBuffer + (blockIndex * DISK_BLOCK_SIZE), blockBuffer, DISK_BLOCK_SIZE); // Copy data into file buffer
        ++blockIndex;
    }
    memcpy(buf, fileDataBuffer + rwPointer, bytesToRead);
    openFDTCache.read_writePointers[fd] = rwPointer + bytesToRead;

    free(indirectBlock);
    free(fileDataBuffer);
    free(blockBuffer);

    return bytesToRead;
}

int sfs_fseek(int fd, int location) {
    /**************ERROR CHECKING**************/
    if (location < 0 || location > iNodesTableCache.iNodes[fd].size) {
        printf("ERROR in sfs_fseek: location is out of file size bounds.\n");
        return fSeekError;
    }

    if (fd < 0 || fd >= TOTAL_FILES || openFDTCache.read_writePointers[fd] < 0) {
        printf("ERROR in sfs_fread: invalid file descriptor.\n");
        return fSeekError;
    }

    /**************FUNCTION**************/
    openFDTCache.read_writePointers[fd] = location;

    return NoError;
}

int sfs_remove(char *fname) {
    /**************ERROR CHECKING**************/
    int lenName = strlen(fname);
    if (lenName < 1 || lenName > MAX_FILENAME_LENGTH) {
        printf("ERROR in sfs_fremove: invalid filename - exceeds bounds.\n");
        return fRemoveError;
    }

    /**************FUNCTION**************/
    int fileIndex = 0;
    int blockIndex = 0;
    IndirectBlock *indirectBlock;
    while (fileIndex < TOTAL_FILES) {
        if (strcmp(rootDirectoryCache.directoryEntries[fileIndex].filename, fname) == 0) {
            iNodesTableCache.iNodes[fileIndex].mode = -1;
            iNodesTableCache.iNodes[fileIndex].linkCount = -1;
            iNodesTableCache.iNodes[fileIndex].uid = -1;
            iNodesTableCache.iNodes[fileIndex].gid = -1;
            iNodesTableCache.iNodes[fileIndex].size = -1;

            for (int directPointerIndex = 0; directPointerIndex < DIRECT_POINTERS; directPointerIndex++) {
                blockIndex = iNodesTableCache.iNodes[fileIndex].directPointers[directPointerIndex];
                if (blockIndex != -1) {
                    freeBlockListCache.data[blockIndex] = FreeBlock;
                }
                iNodesTableCache.iNodes[fileIndex].directPointers[directPointerIndex] = -1;
            }

            if (iNodesTableCache.iNodes[fileIndex].indirectPointer != -1) {
                indirectBlock = read_block(iNodesTableCache.iNodes[fileIndex].indirectPointer);
                for (int indirectPointerIndex = 0; indirectPointerIndex < INDIRECT_POINTERS; indirectPointerIndex++) {
                    blockIndex = indirectBlock->blockOfPointers[indirectPointerIndex];
                    if (blockIndex != -1) {
                        freeBlockListCache.data[blockIndex] = FreeBlock;
                    }
                }
                iNodesTableCache.iNodes[fileIndex].indirectPointer = -1;
                free(indirectBlock);
            }
            openFDTCache.read_writePointers[fileIndex] = -1;
            rootDirectoryCache.directoryEntries[fileIndex].filename[0] = EMPTY_STRING;

            write_blocks(RootDirectoryIndex, TOTAL_ROOT_DIRECTORY_BLOCKS, &rootDirectoryCache);
            write_blocks(FreeBlockListIndex, 1, &freeBlockListCache);
            flush_inode_table_to_disk();

            return NoError;
        }
        ++fileIndex;
    }
    printf("ERROR in sfs_fremove: file to remove is not found in the root directory.\n");
    return fRemoveError;
}