#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <openssl/md5.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include "sharedMemory.h"

int main(int argc, char *argv[])
{
    // checking if the error probability meets the criteria.
    float error_probability;
    if (argc < 2)
    {
        printf("Error! You must enter the error probability!\n");
        printf("Please try writting something like \"./channel.out (int)\" where (int) has to be an integer from 0 to 99!\n");
        exit(1);
    }
    else if (argc > 2)
    {
        printf("Error! You must enter only the error probability! No other arguments!\n");
        printf("Please try writting something like \"./channel.out (int)\" where (int) has to be an integer from 0 to 99!\n");
        exit(1);
    }

    // Checking if th error probability is a number
    char *error = argv[1];
    for (int i = 0, n = strlen(error); i < n; i++)
    {
        if (!isdigit(error[i]))
        {
            printf("Error! The error probability can only be a number!\n");
            printf("Please try writting something like \" ./channel.out \'int\' \" where \'int\' has to be an integer from 0 to 99!\n");
            exit(1);
        }
    }

    // Checking if it is a number between 0 and 99
    error_probability = atoi(error);
    if (!(error_probability >= 0 && error_probability <= 99))
    {
        printf("Error! The error probability has to be a number between 0 and 99!\n");
        printf("Please try writting something like \" ./channel.out \'int\' \" where \'int\' has to be an integer from 0 to 99!\n");
        exit(1);
    }
    error_probability /= 100;

    printf("The error probability has been inserted correctly and it is: %d%% \n", (int)(error_probability * 100));
    // Flag to check whitch proccess (P1 P2) is sender and which is receiver
    // First message goes from P1 to P2
    bool P1_to_P2 = true;
    bool P2_to_P1 = false;

    // Unlinking semaphore names before they are opened.
    sem_unlink(SEM_ENC1_PROD);
    sem_unlink(SEM_CHANNEL_CONS1);

    // Semaphores for enc1 to channel communication
    sem_t *sem_enc1_prod = sem_open(SEM_ENC1_PROD, O_CREAT, 0660, 0);
    if (sem_enc1_prod == SEM_FAILED)
    {
        perror("sem_open/enc1 producer failed");
        exit(EXIT_FAILURE);
    }

    sem_t *sem_channel_cons1 = sem_open(SEM_CHANNEL_CONS1, O_CREAT, 0660, 1);
    if (sem_channel_cons1 == SEM_FAILED)
    {
        perror("sem_open/channel consumer1 failed");
        exit(EXIT_FAILURE);
    }

    // Unlinking semaphore names before they are opened.
    sem_unlink(SEM_ENC2_PROD);
    sem_unlink(SEM_CHANNEL_CONS2);

    // Semaphores for enc2 to channel communication
    sem_t *sem_enc2_prod = sem_open(SEM_ENC2_PROD, O_CREAT, 0660, 0);
    if (sem_enc2_prod == SEM_FAILED)
    {
        perror("sem_open/enc2 producer failed");
        exit(EXIT_FAILURE);
    }

    sem_t *sem_channel_cons2 = sem_open(SEM_CHANNEL_CONS2, O_CREAT, 0660, 1);
    if (sem_channel_cons2 == SEM_FAILED)
    {
        perror("sem_open/channel consumer2 failed");
        exit(EXIT_FAILURE);
    }

    // Variable where the message is strored.
    char message[BLOCK_SIZE];

    // Allocating end string
    char *end_string = malloc(sizeof(char) * BLOCK_SIZE);
    do
    {
        //----------------Case of message from P1 to P2----------------//

        if (P1_to_P2 && !P2_to_P1)
        {
            // Holds a check number to see if the message was transmited and
            // received succesfully only then will the proccess channel will continue.
            char check[2];
            do
            {
                memset(message, 0, BLOCK_SIZE);

                // Attaching shared memory block between Enc1 and channel
                // to receive message from enc1.
                char *block_enc1_ch = attach_memory_block(BLOCK_ENC1_CH, BLOCK_SIZE);
                if (block_enc1_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Receiving message from enc1.
                sem_wait(sem_enc1_prod);
                strncpy(message, block_enc1_ch, BLOCK_SIZE);
                sem_post(sem_channel_cons1);
                detach_memory_block(block_enc1_ch);

                // Allocating end_string so that if it is equal to (TERMINATION_STRING)
                // the proccess will end.
                free(end_string);
                int i = 0;
                for (i; i < strlen(message); i++)
                {
                    if (message[i] == '~')
                    {
                        break;
                    }
                }
                end_string = malloc(sizeof(char) * i);
                strncpy(end_string, message, i);

                // Altering the message if the calculated random number is lower than the probability.
                float error = ((double)rand() / (double)RAND_MAX);

                if (error < error_probability)
                    message[0] = message[0] + 1;
                // End of altering.

                // Semaphores for channel to enc2 communication.
                sem_t *sem_enc2_cons = sem_open(SEM_ENC2_CONS, 0);
                if (sem_enc2_cons == SEM_FAILED)
                {
                    perror("sem_open/enc2 consumer failed");
                    exit(EXIT_FAILURE);
                }
                sem_t *sem_channel_prod2 = sem_open(SEM_CHANNEL_PROD2, 0);
                if (sem_channel_prod2 == SEM_FAILED)
                {
                    perror("sem_open/channel producer2 failed");
                    exit(EXIT_FAILURE);
                }

                // Attaching shared memory block between Enc2 and channel
                // so that channel can send the message to enc2.
                char *block_enc2_ch = attach_memory_block(BLOCK_ENC2_CH, BLOCK_SIZE);
                if (block_enc2_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Sending message to enc2.
                sem_wait(sem_enc2_cons);
                strncpy(block_enc2_ch, message, BLOCK_SIZE);
                sem_post(sem_channel_prod2);
                detach_memory_block(block_enc2_ch);

                // Reattaching shared memory block between Enc2 and channel so that channel
                //  can receive the check value from Enc2 to see if it was sent unchanged.
                block_enc2_ch = attach_memory_block(BLOCK_ENC2_CH, BLOCK_SIZE);
                if (block_enc2_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Receiving the check value from Enc2.
                sem_wait(sem_enc2_prod);
                strncpy(check, block_enc2_ch, BLOCK_SIZE);
                sem_post(sem_channel_cons2);
                detach_memory_block(block_enc2_ch);

                // Semaphores for channel to enc1 communication.
                sem_t *sem_enc1_cons = sem_open(SEM_ENC1_CONS, 0);
                if (sem_enc1_cons == SEM_FAILED)
                {
                    perror("sem_open/enc1 consumer failed");
                    exit(EXIT_FAILURE);
                }
                sem_t *sem_channel_prod1 = sem_open(SEM_CHANNEL_PROD1, 0);
                if (sem_channel_prod1 == SEM_FAILED)
                {
                    perror("sem_open/channel producer1 failed");
                    exit(EXIT_FAILURE);
                }

                // Attaching shared memory block between Enc1 and channel so that
                // channel can send to Enc1 the check value.
                block_enc1_ch = attach_memory_block(BLOCK_ENC1_CH, BLOCK_SIZE);
                if (block_enc1_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Sending check value to enc1.
                sem_wait(sem_enc1_cons);
                strncpy(block_enc1_ch, check, BLOCK_SIZE);
                sem_post(sem_channel_prod1);
                detach_memory_block(block_enc1_ch);

                //  If the check value is "1" the proccess can continue otherwise it will
                //  wait to receive the message from Enc1 so that it can be transmitted to Enc2.
                //  This will continue until the check value is "1" that is until the
                //  message is sent unchanged by the channel.
            } while (check[0] == '0');

            //  Next P2 is the sender and P1 the receiver.
            P1_to_P2 = false;
            P2_to_P1 = true;
        }
        else
        {
            //----------------Case of message from P2 to P1----------------//

            // Holds a check number to see if the message was transmited and
            // received succesfully only then will the proccess channel will continue.
            char check[2];
            do
            {
                memset(message, 0, BLOCK_SIZE);

                // Attaching shared memory block between Enc2 and channel
                // to receive message from Enc2.
                char *block_enc2_ch = attach_memory_block(BLOCK_ENC2_CH, BLOCK_SIZE);
                if (block_enc2_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Receiving message from enc2.
                sem_wait(sem_enc2_prod);
                strncpy(message, block_enc2_ch, BLOCK_SIZE);
                sem_post(sem_channel_cons2);
                detach_memory_block(block_enc2_ch);

                // Allocating end_string so that if it is equal to (TERMINATION_STRING)
                // the proccess will end.
                free(end_string);
                int i = 0;
                for (i; i < strlen(message); i++)
                {
                    if (message[i] == '~')
                    {
                        break;
                    }
                }
                end_string = malloc(sizeof(char) * i);
                strncpy(end_string, message, i);
                // Altering the message if the calculated random number is lower than the probability.
                float current = ((double)rand() / (double)RAND_MAX);

                if (current < error_probability)
                    message[0] = message[0] + 1;
                // End of altering.

                // Semaphores for channel to enc1 communication.
                sem_t *sem_enc1_cons = sem_open(SEM_ENC1_CONS, 0);
                if (sem_enc1_cons == SEM_FAILED)
                {
                    perror("sem_open/enc1 consumer failed");
                    exit(EXIT_FAILURE);
                }
                sem_t *sem_channel_prod1 = sem_open(SEM_CHANNEL_PROD1, 0);
                if (sem_channel_prod1 == SEM_FAILED)
                {
                    perror("sem_open/channel producer1 failed");
                    exit(EXIT_FAILURE);
                }

                // Attaching shared memory block between Enc1 and channel
                // so that channel can send the message to enc1.
                char *block_enc1_ch = attach_memory_block(BLOCK_ENC1_CH, BLOCK_SIZE);
                if (block_enc1_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Sending message to enc1.
                sem_wait(sem_enc1_cons);
                strncpy(block_enc1_ch, message, BLOCK_SIZE);
                sem_post(sem_channel_prod1);
                detach_memory_block(block_enc1_ch);

                // Reattaching shared memory block between Enc1 and channel so that channel
                //  can receive the check value from Enc1 to see if it was sent unchanged.
                block_enc1_ch = attach_memory_block(BLOCK_ENC1_CH, BLOCK_SIZE);
                if (block_enc1_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Receiving the check value from Enc1.
                sem_wait(sem_enc1_prod);
                strncpy(check, block_enc1_ch, BLOCK_SIZE);
                sem_post(sem_channel_cons1);
                detach_memory_block(block_enc1_ch);

                // Semaphores for channel to Enc2 communication.
                sem_t *sem_enc2_cons = sem_open(SEM_ENC2_CONS, 0);
                if (sem_enc2_cons == SEM_FAILED)
                {
                    perror("sem_open/enc2 consumer failed");
                    exit(EXIT_FAILURE);
                }
                sem_t *sem_channel_prod2 = sem_open(SEM_CHANNEL_PROD2, 0);
                if (sem_channel_prod2 == SEM_FAILED)
                {
                    perror("sem_open/channel producer2 failed");
                    exit(EXIT_FAILURE);
                }

                // Attaching shared memory block between Enc2 and channel so that
                // channel can send to Enc2 the check value.
                block_enc2_ch = attach_memory_block(BLOCK_ENC2_CH, BLOCK_SIZE);
                if (block_enc2_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Sending check value to Enc2.
                sem_wait(sem_enc2_cons);
                strncpy(block_enc2_ch, check, BLOCK_SIZE);
                sem_post(sem_channel_prod2);
                detach_memory_block(block_enc2_ch);

                // If the check value is "1" the proccess can continue otherwise it will
                // wait to receive the message from Enc2 so that it can be transmitted to Enc2.
                // This will continue until the check value is "1" that is until the
                // message is sent unchanged by the channel.
            } while (check[0] == '0');

            // Next P2 is the sender and P1 the receiver.
            P1_to_P2 = true;
            P2_to_P1 = false;
        }

        // Checking if the message sent is the (TERMINATION_STRING).
    } while (strcmp(end_string, TERMINATION_STRING));

    //Printing that channel is ending
    printf("channel proccess ending\n");

    // Closing allocated semaphores and unlinking their names.
    sem_close(sem_enc1_prod);
    sem_close(sem_channel_cons1);
    sem_close(sem_enc2_prod);
    sem_close(sem_channel_cons2);
    sem_unlink(SEM_ENC1_PROD);
    sem_unlink(SEM_CHANNEL_CONS1);
    sem_unlink(SEM_ENC2_PROD);
    sem_unlink(SEM_CHANNEL_CONS2);

    return 0;
}
