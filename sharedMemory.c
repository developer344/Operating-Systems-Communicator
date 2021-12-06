#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "sharedMemory.h"

// Helps check for Ipc result errors
#define IPC_RESULT_ERROR (-1)

// Takes the key of the memory block uses it to get hold of the memory.
static int get_shared_block(char *filename, int size)
{
    key_t key;

    key = ftok(filename, 0);
    if (key == IPC_RESULT_ERROR)
        return IPC_RESULT_ERROR;
    return shmget(key, size, 0644 | IPC_CREAT);
}

// Function that attaches the shared memory block to a pointer
// in the main code.
char *attach_memory_block(char *filename, int size)
{
    int shared_block_id = get_shared_block(filename, size);
    char *result;
    if (shared_block_id == IPC_RESULT_ERROR)
        return NULL;

    result = shmat(shared_block_id, NULL, 0);
    if (result == (char *)IPC_RESULT_ERROR)
        return NULL;

    return result;
}

//Function that detaches the shared memory block from the pointer
bool detach_memory_block(char *block)
{
    return (shmdt(block) != IPC_RESULT_ERROR);
}

//Function that destroys the shared memory block
bool destroy_memory_block(char *filename)
{
    int shared_block_id = get_shared_block(filename, 0);

    if (shared_block_id == IPC_RESULT_ERROR)
        return NULL;

    return (shmctl(shared_block_id, IPC_RMID, NULL) != IPC_RESULT_ERROR);
}