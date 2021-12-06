#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
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
    //Child key
    int enc1_pid;

    // Flag to check whitch proccess (P1 P2) is sender and which is receiver
    // First sender is P1
    bool P1Sender = true;

    // Unlinking semaphore names before they are opened.
    sem_unlink(SEM_PRODUCER_P1);
    sem_unlink(SEM_CONSUMER_ENC1);

    // Semaphores for P1 to enc1 communication
    sem_t *sem_enc1_cons_p1 = sem_open(SEM_CONSUMER_ENC1, O_CREAT | O_EXCL, 0660, 1);
    if (sem_enc1_cons_p1 == SEM_FAILED)
    {
        perror("sem_open/Enc1 consumer failed");
        exit(EXIT_FAILURE);
    }
    sem_t *sem_p1_prod_enc1 = sem_open(SEM_PRODUCER_P1, O_CREAT | O_EXCL, 0660, 0);
    if (sem_p1_prod_enc1 == SEM_FAILED)
    {
        perror("sem_open/P1 producer failed");
        exit(EXIT_FAILURE);
    }

    // Unlinking semaphore names before they are opened.
    sem_unlink(SEM_PRODUCER_ENC1);
    sem_unlink(SEM_CONSUMER_P1);

    // Semaphores for enc1 to P1 communication
    sem_t *sem_enc1_prod_p1 = sem_open(SEM_PRODUCER_ENC1, O_CREAT | O_EXCL, 0660, 1);
    if (sem_enc1_prod_p1 == SEM_FAILED)
    {
        perror("sem_open/Enc1 producer failed");
        exit(EXIT_FAILURE);
    }
    sem_t *sem_p1_cons_enc1 = sem_open(SEM_CONSUMER_P1, O_CREAT | O_EXCL, 0660, 0);
    if (sem_p1_cons_enc1 == SEM_FAILED)
    {
        perror("sem_open/P1 consumer failed");
        exit(EXIT_FAILURE);
    }

    // Unlinking semaphore names before they are opened.
    sem_unlink(SEM_ENC1_CONS);
    sem_unlink(SEM_CHANNEL_PROD1);

    // Fork for creating Enc1.
    if ((enc1_pid = fork()) == 0)
    {
        //------------------------------Enc1 proccess------------------------------//

        // Declaring all needed variables.
        char message[BLOCK_SIZE];
        char result[BLOCK_SIZE];
        char *check_sum;
        const char *receivedcheck_sum;

        // Here the information of the message is stored to see is if it is
        // equal to the TERMINATION_STRING.
        char *end_string = malloc(sizeof(char) * BLOCK_SIZE);

        // Semaphores for channel to enc1 comunication
        sem_t *sem_enc1_cons = sem_open(SEM_ENC1_CONS, O_CREAT | O_EXCL, 0660, 1);
        if (sem_enc1_cons == SEM_FAILED)
        {

            perror("sem_open/enc1 consumer failed");
            exit(EXIT_FAILURE);
        }
        sem_t *sem_channel_prod1 = sem_open(SEM_CHANNEL_PROD1, O_CREAT | O_EXCL, 0660, 0);
        if (sem_channel_prod1 == SEM_FAILED)
        {
            perror("sem_open/channel producer1 failed");
            exit(EXIT_FAILURE);
        }

        do
        {
            if (P1Sender)
            {
                //-------------Enc1 when it sends a message to channel-------------//

                // Check stores the check value to see if the message was sent unchanged
                char check[2];
                do
                {
                    memset(message, 0, BLOCK_SIZE);

                    // Attaching shared memory block between P1 and Enc2 so that Enc2
                    // can receive the message from P1.
                    char *block_p1_enc1 = attach_memory_block(BLOCK_P1_ENC1, BLOCK_SIZE);
                    if (block_p1_enc1 == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Receiving the message from P2.
                    sem_wait(sem_p1_prod_enc1);
                    strncpy(message, block_p1_enc1, BLOCK_SIZE);
                    sem_post(sem_enc1_cons_p1);
                    detach_memory_block(block_p1_enc1);

                    // Allocating end_string so that if it is equal to (TERMINATION_STRING)
                    // the proccess will end.
                    free(end_string);
                    end_string = malloc(sizeof(char) * BLOCK_SIZE);
                    strcpy(end_string, message);

                    // Calculating the checksum and adding it next the message after the
                    // spesial character "~" so that they can be extracted by Enc1 later.
                    char *check_sum = malloc(sizeof(char) * MD5_DIGEST_LENGTH);
                    MD5(message, sizeof(message), check_sum);
                    strcat(message, "~");
                    strcat(message, check_sum);
                    free(check_sum);
                    //End of checksum calculation.

                    // Semaphores for Enc1 to channel communication.
                    sem_t *sem_enc1_prod = sem_open(SEM_ENC1_PROD, 0);
                    if (sem_enc1_prod == SEM_FAILED)
                    {
                        perror("sem_open/enc1 producer failed");
                        exit(EXIT_FAILURE);
                    }
                    sem_t *sem_channel_cons1 = sem_open(SEM_CHANNEL_CONS1, 0);
                    if (sem_channel_cons1 == SEM_FAILED)
                    {
                        perror("sem_open/channel consumer1 failed");
                        exit(EXIT_FAILURE);
                    }

                    // Attaching shared memory block between Enc1 and channel so that
                    // the message along with the checksum can be sent to channel.
                    char *block_enc1_ch = attach_memory_block(BLOCK_ENC1_CH, BLOCK_SIZE);
                    if (block_enc1_ch == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Sending the message to channel.
                    sem_wait(sem_channel_cons1);
                    strncpy(block_enc1_ch, message, BLOCK_SIZE);
                    sem_post(sem_enc1_prod);
                    detach_memory_block(block_enc1_ch);

                    // Attaching shared memory block between Enc1 and channel so that Enc1 can
                    // receive the check value to see if the message was sent unchanged.
                    block_enc1_ch = attach_memory_block(BLOCK_ENC1_CH, BLOCK_SIZE);
                    if (block_enc1_ch == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Receiving check value from channel.
                    sem_wait(sem_channel_prod1);
                    strncpy(check, block_enc1_ch, BLOCK_SIZE);
                    sem_post(sem_enc1_cons);
                    detach_memory_block(block_enc1_ch);

                    // Attaching shared memory block between Enc1 and P1 so that Enc1 can send
                    // the check value to P1.
                    block_p1_enc1 = attach_memory_block(BLOCK_P1_ENC1, BLOCK_SIZE);
                    if (block_p1_enc1 == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Sending the check value to P1.
                    sem_wait(sem_enc1_prod_p1);
                    strncpy(block_p1_enc1, check, BLOCK_SIZE);
                    sem_post(sem_p1_cons_enc1);
                    detach_memory_block(block_p1_enc1);

                    // If the check value is "1". Enc1 will be allowed to continue otherwise
                    // it will continue receiving the message from P1 and sending it to Enc2
                    // through the channel until it is sent unchanged.
                } while ((check[0] == '0'));

                // Next P1 is the receiver and P2 is the sender.
                P1Sender = false;
            }
            else
            {
                //-------------Enc2 when it receives a message from channel-------------//
                memset(message, 0, BLOCK_SIZE);
                memset(result, 0, BLOCK_SIZE);

                // Attaching shared memory block between Enc1 and channel so that the
                // message from the channel can be received.
                char *block_enc1_ch = attach_memory_block(BLOCK_ENC1_CH, BLOCK_SIZE);
                if (block_enc1_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Receiving the message from P1 through Enc1 and channel.
                sem_wait(sem_channel_prod1);
                strncpy(message, block_enc1_ch, BLOCK_SIZE);
                sem_post(sem_enc1_cons);
                detach_memory_block(block_enc1_ch);
                printf("Message: %s\n", message);
                // Extracting the information from the received message
                // and copying it to the (result).
                int i = 0;
                for (i; i < strlen(message); i++)
                {
                    if (message[i] == '~')
                    {
                        break;
                    }
                }
                strncpy(result, message, i);

                // Extracting the check_sum that was received.
                receivedcheck_sum = &message[i + 1];

                // Calculating the check sum of the message.
                check_sum = malloc(sizeof(char) * MD5_DIGEST_LENGTH);
                MD5(result, sizeof(result), check_sum);

                // Checking if the received checksum and the calculated checksum match
                // to see if the message was transmitted through the channel correctly.
                while (strcmp(receivedcheck_sum, check_sum))
                {
                    // If not, I send the error value of "0" back to P1.
                    memset(message, 0, BLOCK_SIZE);
                    memset(result, 0, BLOCK_SIZE);

                    // Semaphores for Enc1 to channel communication.
                    sem_t *sem_enc1_prod = sem_open(SEM_ENC1_PROD, 0);
                    if (sem_enc1_prod == SEM_FAILED)
                    {
                        perror("sem_open/enc1 producer failed");
                        exit(EXIT_FAILURE);
                    }
                    sem_t *sem_channel_cons1 = sem_open(SEM_CHANNEL_CONS1, 0);
                    if (sem_channel_cons1 == SEM_FAILED)
                    {
                        perror("sem_open/channel consumer1 failed");
                        exit(EXIT_FAILURE);
                    }

                    // Attaching shared memory block between Enc2 and channel.
                    char *block_enc1_ch = attach_memory_block(BLOCK_ENC1_CH, BLOCK_SIZE);
                    if (block_enc1_ch == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Sending the error value "0" back to P2 through channel and Enc2
                    // so that P2 can know that the message that was transmited changed
                    // in the channel and the message will have to be resent by P2.
                    sem_wait(sem_channel_cons1);
                    strncpy(block_enc1_ch, "0", BLOCK_SIZE);
                    sem_post(sem_enc1_prod);
                    detach_memory_block(block_enc1_ch);

                    // Attaching shared memory block between Enc1 and channel
                    // so that Enc1 can receive the resent message from P2.
                    block_enc1_ch = attach_memory_block(BLOCK_ENC1_CH, BLOCK_SIZE);
                    if (block_enc1_ch == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Receiving the resent message from P1.
                    sem_wait(sem_channel_prod1);
                    strncpy(message, block_enc1_ch, BLOCK_SIZE);
                    sem_post(sem_enc1_cons);
                    detach_memory_block(block_enc1_ch);

                    // Extracting the information from the received message
                    // and copying it to the (result).
                    int i = 0;
                    for (i; i < strlen(message); i++)
                    {
                        if (message[i] == '~')
                        {
                            break;
                        }
                    }
                    strncpy(result, message, i);

                    // Extracting the check sum from the message
                    receivedcheck_sum = &message[i + 1];

                    // Calculating the new check sum of the received message
                    free(check_sum);
                    check_sum = malloc(sizeof(char) * MD5_DIGEST_LENGTH);
                    MD5(result, sizeof(result), check_sum);

                    // If the new checksum and the checksum that I extracted from the message
                    // match it means that the message that we have received has not been altered
                    // by the channel and we are allowed to continue. If not I repeat the previous
                    // procedure until the received checksum and the calculated check sum match.
                }

                // Freeing check sum from allocated memory.
                free(check_sum);

                // Semaphores for Enc1 to channel communication.
                sem_t *sem_enc1_prod = sem_open(SEM_ENC1_PROD, 0);
                if (sem_enc1_prod == SEM_FAILED)
                {
                    perror("sem_open/enc1 producer failed");
                    exit(EXIT_FAILURE);
                }
                sem_t *sem_channel_cons1 = sem_open(SEM_CHANNEL_CONS1, 0);
                if (sem_channel_cons1 == SEM_FAILED)
                {
                    perror("sem_open/channel consumer1 failed");
                    exit(EXIT_FAILURE);
                }

                // Attaching shared memory block between channel and Enc1.
                block_enc1_ch = attach_memory_block(BLOCK_ENC1_CH, BLOCK_SIZE);
                if (block_enc1_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // After the end of the previous loop the message has been correctly
                // transmitted and a check value of "1" is sent back to all channel,
                // Enc2 and P2 so that they can continue.
                sem_wait(sem_channel_cons1);
                strncpy(block_enc1_ch, "1", BLOCK_SIZE);
                sem_post(sem_enc1_prod);
                detach_memory_block(block_enc1_ch);

                // Copying the information that I extracted to the message
                // so that it can be sent to P1.
                memset(message, 0, BLOCK_SIZE);
                strcpy(message, result);

                // Attaching shared memory block between Enc1 and P1 so that the message
                // can me sent to P1.
                char *block_p1_enc1 = attach_memory_block(BLOCK_P1_ENC1, BLOCK_SIZE);
                if (block_p1_enc1 == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Sending final correct message from Enc1 to P1.
                sem_wait(sem_enc1_prod_p1);
                strncpy(block_p1_enc1, message, BLOCK_SIZE);
                sem_post(sem_p1_cons_enc1);
                detach_memory_block(block_p1_enc1);

                // Allocating end_string so that if it is equal to (TERMINATION_STRING)
                // the proccess will end.
                free(end_string);
                end_string = malloc(sizeof(char) * BLOCK_SIZE);
                strcpy(end_string, message);

                // Next P1 is the sender and P2 the receiver.
                P1Sender = true;
            }

            // Checking if the message sent is the (TERMINATION_STRING).
        } while (strcmp(end_string, TERMINATION_STRING));

        //Printing that Enc1 is ending
        printf("Enc1 proccess ending\n");

        //Closing allocated semaphores and unlinking their names
        sem_close(sem_enc1_cons);
        sem_close(sem_channel_prod1);
        sem_unlink(SEM_ENC1_CONS);
        sem_unlink(SEM_CHANNEL_PROD1);

        // Freeing all allocated memory from pointers.
        free(end_string);
    }
    else
    {
        //------------------------------P1 proccess------------------------------//

        // Declaring needed variables.
        char message[BLOCK_SIZE];

        // Here the information of the message is stored to see is if it is
        // equal to the TERMINATION_STRING.
        char *end_string = malloc(sizeof(char) * BLOCK_SIZE);
        do
        {
            // Check stores the check value to see if the message was sent unchanged.
            char check[2];
            if (P1Sender)
            {
                //--------------P1 when it sends a message to P2--------------//

                memset(message, 0, BLOCK_SIZE);

                // Reading the message from input.
                printf("User P1:");
                fgets(message, BLOCK_SIZE, stdin);
                strtok(message, "\n");
                do
                {
                    // Attaching shared memory block between P1 and Enc1.
                    // so that the message can be sent to Enc1.
                    char *block_p1_enc1 = attach_memory_block(BLOCK_P1_ENC1, BLOCK_SIZE);
                    if (block_p1_enc1 == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Sending the message to Enc1.
                    sem_wait(sem_enc1_cons_p1);
                    strncpy(block_p1_enc1, message, BLOCK_SIZE);
                    sem_post(sem_p1_prod_enc1);
                    detach_memory_block(block_p1_enc1);

                    // Attaching shared memory block between P1 and Enc1.
                    // so that the check value can be received.
                    block_p1_enc1 = attach_memory_block(BLOCK_P1_ENC1, BLOCK_SIZE);
                    if (block_p1_enc1 == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Receiving the check value from Enc1.
                    sem_wait(sem_p1_cons_enc1);
                    strncpy(check, block_p1_enc1, BLOCK_SIZE);
                    sem_post(sem_enc1_prod_p1);
                    detach_memory_block(block_p1_enc1);

                    // If the check value is "1" the proccess can continue otherwise it will
                    // resend the message to Enc1 so that it can be transmitted to P2.
                    // This will continue until the check value is "1" that is until the
                    // message is sent unchanged by the channel.
                } while ((check[0] == '0'));

                // Next P1 is the receiver and P2 is the sender.
                P1Sender = false;

                // Allocating end_string so that if it is equal to (TERMINATION_STRING)
                // the proccess will end.
                free(end_string);
                end_string = malloc(sizeof(char) * BLOCK_SIZE);
                strcpy(end_string, message);
            }
            else
            {
                //--------------P1 when it receives a message from P2--------------//

                memset(message, 0, BLOCK_SIZE);

                // Attaching shared memory block between P1 and Enc1 so that
                // the message can be received from Enc1 by P1.
                char *block_p1_enc1 = attach_memory_block(BLOCK_P1_ENC1, BLOCK_SIZE);
                if (block_p1_enc1 == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Receiving message from Enc1.
                sem_wait(sem_p1_cons_enc1);
                strncpy(message, block_p1_enc1, BLOCK_SIZE);

                // Printing received message from P2.
                printf("Message from user P2:%s\n", message);
                sem_post(sem_enc1_prod_p1);
                detach_memory_block(block_p1_enc1);

                // Next P1 is the sender and P2 the receiver.
                P1Sender = true;

                // Allocating end_string so that if it is equal to (TERMINATION_STRING)
                // the proccess will end.
                free(end_string);
                end_string = malloc(sizeof(char) * BLOCK_SIZE);
                strcpy(end_string, message);
            }

            // Checking if its the (TERMINATION_STRING)
        } while (strcmp(end_string, TERMINATION_STRING));

        // Printing that P1 is ending
        printf("P1 proccess ending\n");
    }
    waitpid(enc1_pid, NULL, 0);
    // Destroying all allocated shared memory blocks
    destroy_memory_block(BLOCK_P1_ENC1);
    destroy_memory_block(BLOCK_ENC1_CH);

    // Closing allocated semaphores and unlinking their names.
    sem_close(sem_enc1_cons_p1);
    sem_close(sem_enc1_prod_p1);
    sem_close(sem_p1_cons_enc1);
    sem_close(sem_p1_prod_enc1);

    sem_unlink(SEM_PRODUCER_P1);
    sem_unlink(SEM_CONSUMER_ENC1);
    sem_unlink(SEM_PRODUCER_ENC1);
    sem_unlink(SEM_CONSUMER_P1);

    return 0;
}