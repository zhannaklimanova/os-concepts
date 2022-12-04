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

int isFileDescriptorInvalid(int fd)
{
    if (fd < 0 || fd >= TOTAL_FILES || openFDTCache.readPointers[fd] < 0 || openFDTCache.writePointers[fd] < 0) {
        return 1;
    }
    return 0;
}

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

        for (int i = 0; i < DISK_BLOCK_SIZE; i++)
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
    int lenName = strlen(fname);
    if (lenName < 1 || lenName > MAX_FILENAME_LENGTH) {
        return -1;
    }

    for (int fileIndex = 0; fileIndex < TOTAL_FILES; fileIndex++)
    {
        int rc = strcmp(fname, rootDirectoryCache.directoryEntries[fileIndex].filename);
        if (rc == 0) {
            openFDTCache.readPointers[fileIndex] = 0; // set pointer to the beginning of file
            openFDTCache.writePointers[fileIndex] = iNodesTableCache.iNodes[fileIndex].size; // set write pointer to end of the file content
            return fileIndex;
        }

        // If the file is not in the root directory, create it
        for (int fileIndex = 0; fileIndex < TOTAL_FILES; fileIndex++)
        {
            if (rootDirectoryCache.directoryEntries[fileIndex].filename[0] == EMPTY_STRING) {
                openFDTCache.readPointers[fileIndex] = 0;
                openFDTCache.writePointers[fileIndex] = 0;
                iNodesTableCache.iNodes[fileIndex].size = 0;
                strncpy(rootDirectoryCache.directoryEntries[fileIndex].filename, fname, MAX_FILENAME_LENGTH);
                // Save updated i-Nodes Table to disk
                void *buffer = (void*) malloc(DISK_BLOCK_SIZE * DIRECT_POINTERS); // CHANGE
                memcpy(buffer, &iNodesTableCache, DISK_BLOCK_SIZE * DIRECT_POINTERS); // CHANGE
                write_blocks(iNodeTableIndex, DIRECT_POINTERS, buffer); // CHANGE
                free(buffer); // CHANGE

                // Save updated root directory to disk
                write_blocks(RootDirectoryIndex, 1, &rootDirectoryCache);
            }
        }
    }

    printf("ERROR in sfs_fopen: not enough space available for creating a new file.");
    return -1;
}

int sfs_fread(int fd, char* buf, int count)
{

    if (isFileDescriptorInvalid(fd)) {
        return -1;  // ERROR: invalid fileID
    }

    int file_size = iNodesTableCache.iNodes[fd].size;
    if (file_size < 0) {
        return 0; // No bytes to read
    }

    int read_ptr = openFDTCache.readPointers[fd]; // Get read pointer
    int read_bytes = count; // # of bytes to read
    if (file_size < read_ptr + count) { // Clip to end of file
        read_bytes = file_size - read_ptr;
        if (read_bytes <= 0){
            return 0; // No bytes to read
        }
    }

    // Compute number of blocks the file takes
    int n_blocks = file_size / DISK_BLOCK_SIZE; // Number of blocks needed to hold file
    if (file_size % DISK_BLOCK_SIZE != 0) {
        n_blocks++;
    }

    char *file_buffer = malloc(n_blocks * DISK_BLOCK_SIZE); // Buffer to cache read file
    IndirectBlock *indirectBlock = NULL;
    char *block_buffer = malloc(DISK_BLOCK_SIZE); // Buffer to cache each read block

    for (int i = 0; i < n_blocks; i++) {  // Loop number of blocks
        int block_i;    // block number
        if (i < DIRECT_POINTERS) {  // Direct block
            block_i = iNodesTableCache.iNodes[fd].directPointers[i];
        } else {  // Indirect block
            if (indirectBlock == NULL) {
                indirectBlock = read_block(iNodesTableCache.iNodes[fd].indirectPointer); // Cache indirect block

            }
            int indirect_i = i - DIRECT_POINTERS; // Indirect index
            if (indirect_i >= INDIRECT_POINTERS) {
                // Free variables
                free(indirectBlock);
                free(file_buffer);
                free(block_buffer);

                return -1; // ERROR: exceeded max size of indirect block
            }
            block_i = indirectBlock->blockOfPointers[indirect_i];
        }
        if (block_i < 0) {
            // Free variables
            free(indirectBlock);
            free(file_buffer);
            free(block_buffer);

            return -1; // ERROR: invalid block number
        }
        memset(block_buffer, 0, DISK_BLOCK_SIZE); // Clear block buffer
        read_blocks(block_i, 1, block_buffer); // Load data into block buffer
        memcpy(file_buffer + (i * DISK_BLOCK_SIZE), block_buffer, DISK_BLOCK_SIZE); // Copy data into file buffer
    }

    memcpy(buf, file_buffer + read_ptr, read_bytes); // Copy data into returned buffer
    // Free variables
    free(indirectBlock);
    free(file_buffer);
    free(block_buffer);

    openFDTCache.readPointers[fd] = read_ptr + read_bytes; // Move read pointer to where we stopped reading

    return read_bytes; // Returns number of bytes read
}

int sfs_fseek(int fd, int location)
{
    if (isFileDescriptorInvalid(fd)) {
        return -1;  // ERROR: invalid fileID
    }

    if (location < 0 || location > iNodesTableCache.iNodes[fd].size) {
        return -1; // ERROR: invalid loc
    }

    openFDTCache.readPointers[fd] = location; // Move read pointer
    openFDTCache.writePointers[fd] = location; // Move write pointer

    return 0;
}

int sfs_fwrite(int fd, const char* buf, int count)
{
    if (isFileDescriptorInvalid(fd)) {
        printf("ERROR in sfs_fclose: file descriptor is invalid.");
        return -1;
    }

    iNode iNodeOfFile = iNodesTableCache.iNodes[fd];

    // Get size of file after write
    int writePointer = openFDTCache.writePointers[fd];
    int sizeAfterWrite = writePointer + count;
    if (sizeAfterWrite < iNodeOfFile.size) {
        sizeAfterWrite = iNodeOfFile.size;
    }

    // Get number blocks file contents will use require
    int numBlocks = sizeAfterWrite / DISK_BLOCK_SIZE;
    if (numBlocks % DISK_BLOCK_SIZE != 0) {
        numBlocks++;
    }

    // Get existing data from file
    char *tempFileBuffer = malloc(numBlocks * DISK_BLOCK_SIZE); // cast (char *)
    sfs_fseek(fd, 0);
    sfs_fread(fd, tempFileBuffer, iNodeOfFile.size);
    sfs_fseek(fd, openFDTCache.readPointers[fd]);
    memcpy(tempFileBuffer + writePointer, buf, count);

    IndirectBlock *indirectBlock = NULL;
    for (int i = 0; i < numBlocks; i++)
    {
        int blockIndex;

        if (i < DIRECT_POINTERS) {
            blockIndex = iNodeOfFile.directPointers[i];
            if (blockIndex < 0) {
                blockIndex = allocateBlock();
                iNodesTableCache.iNodes[fd].directPointers[i] = blockIndex;
            }
        }
        else {
            if (iNodeOfFile.indirectPointer < 0) {
                int indirectBlockIndex = allocateBlock();
                iNodeOfFile.indirectPointer = indirectBlockIndex;
                iNodesTableCache.iNodes[fd].indirectPointer = indirectBlockIndex;
                IndirectBlock indirectBlock;
                for (int j = 0; j < INDIRECT_POINTERS; j++)
                {
                    indirectBlock.blockOfPointers[j] = INITIALIZATION_VALUE;
                }
                blockIndex = allocateBlock();
                indirectBlock.blockOfPointers[0] = blockIndex;
                write_blocks(indirectBlockIndex, 1, &indirectBlock);
            }
            else { // Indirect block has already been initialized
                if (indirectBlock == NULL) {
                    indirectBlock = read_block(iNodeOfFile.indirectPointer);
                }
                int indirectIndex = i - DIRECT_POINTERS;
                if (indirectIndex >= INDIRECT_POINTERS) {
                    free(tempFileBuffer);
                    free(indirectBlock);
                    printf("ERROR in sfs_fwrite: exceeded number of indirect blocks.");
                    return -1;
                }
                blockIndex = indirectBlock->blockOfPointers[indirectIndex];
                if (blockIndex < 0) {
                    blockIndex = allocateBlock();
                    indirectBlock->blockOfPointers[indirectIndex] = blockIndex;
                    write_blocks(iNodeOfFile.indirectPointer, 1, indirectBlock);
                }
            }
        }
        if (blockIndex < 0 || blockIndex >= DISK_DATA_BLOCKS) {
            free(indirectBlock);
            free(tempFileBuffer);
            return -1;
        }
        write_blocks(blockIndex, 1, tempFileBuffer + (i * DISK_DATA_BLOCKS));
    }
    free(indirectBlock);
    free(tempFileBuffer);

    openFDTCache.writePointers[fd] = sizeAfterWrite;
    iNodesTableCache.iNodes[fd].size = sizeAfterWrite;

    void *buffer = (void*) malloc(DISK_BLOCK_SIZE * DIRECT_POINTERS); // CHANGE
    memcpy(buffer, &iNodesTableCache, DISK_BLOCK_SIZE * DIRECT_POINTERS); // CHANGE
    write_blocks(iNodeTableIndex, DIRECT_POINTERS, buffer); // CHANGE
    free(buffer); // CHANGE

    return count; // return number of bytes written
}

int sfs_fclose(int fd)
{
    if (isFileDescriptorInvalid(fd)) {
        printf("ERROR in sfs_fclose: file descriptor is invalid.");
        return -1;
    }
    openFDTCache.readPointers[fd] = INITIALIZATION_VALUE;
    openFDTCache.writePointers[fd] = INITIALIZATION_VALUE;

    return 0;
}

int sfs_remove(char *fname)
{
    for (int i = 0; i < TOTAL_FILES; i++) { // Loop over files
        if (strcmp(fname, rootDirectoryCache.directoryEntries[i].filename) == 0) { // File found (filename matches)
            // Dereference inode data
            iNodesTableCache.iNodes[i].mode = -1;
            iNodesTableCache.iNodes[i].linkCount = -1;
            iNodesTableCache.iNodes[i].uid = -1;
            iNodesTableCache.iNodes[i].gid = -1;
            iNodesTableCache.iNodes[i].size = -1;

            // Direct
            for (int j = 0; j < DIRECT_POINTERS; j++) { // Loop over direct pointers
                int block_i = iNodesTableCache.iNodes[i].directPointers[j];
                if (block_i != -1) {    // Free used direct data blocks
                    freeBlockListCache.data[block_i] = 1;
                }
                iNodesTableCache.iNodes[i].directPointers[j] = -1;   // Dereference direct pointers
            }

            // Indirect
            if (iNodesTableCache.iNodes[i].indirectPointer != -1) {
                IndirectBlock *indirectBlock = read_block(iNodesTableCache.iNodes[i].indirectPointer);  // Get indirect block
                for (int i = 0; i < INDIRECT_POINTERS; i++) { // Loop over indirect pointers
                    int block_i = indirectBlock->blockOfPointers[i];
                    if (block_i != -1) {    // Free used indirect data blocks
                        freeBlockListCache.data[block_i] = 1;
                    }
                }
                iNodesTableCache.iNodes[i].indirectPointer = -1;    // Dereference indirect pointers
                free(indirectBlock);
            }

            openFDTCache.readPointers[i] = -1;    // Clear read pointer
            openFDTCache.writePointers[i] = -1;   // Clear write pointer
            rootDirectoryCache.directoryEntries[i].filename[0] = '\0'; // Clear filename

            flush_root_directory_to_disk();  // Update root directory in disk
            flush_fbm_to_disk(); // Update free bit map in disk
            flush_inode_table_to_disk(); // Update inode table in disk

            return 0;
        }
    }

    return -1; // ERROR: file not found
}

int sfs_getnextfilename(char* fname)
{
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

int sfs_getfilesize(const char* path)
{
    if (strlen(path) < 1 || strlen(path) > MAX_FILENAME_LENGTH)
        return -1; // ERROR: invalid path

    for (int i = 0; i < TOTAL_FILES; i++) {   // Loop over files
        if (strcmp(rootDirectoryCache.directoryEntries[i].filename, path) == 0) { // File found (matches path)
            int size = iNodesTableCache.iNodes[i].size;  // Get size of inode

            return size;    // Returns size of file
        }
    }

    return -1;  // ERROR: File doesn't exist
}