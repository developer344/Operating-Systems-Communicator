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

#include "sharedMemory.h"

int main(int argc, char *argv[])
{
    // Child key that is Enc2 key.
    int enc2_pid;

    // Flag to check whitch proccess (P1 P2) is sender and which is receiver.
    // First receiver is P2
    bool P2Sender = false;

    // Unlinking semaphore names before they are opened.
    sem_unlink(SEM_PRODUCER_ENC2);
    sem_unlink(SEM_CONSUMER_P2);

    // Semaphores for enc2 to P2 communication.
    sem_t *sem_enc2_prod_p2 = sem_open(SEM_PRODUCER_ENC2, O_CREAT | O_EXCL, 0660, 1);
    if (sem_enc2_prod_p2 == SEM_FAILED)
    {
        perror("sem_open/enc2 producer failed");
        exit(EXIT_FAILURE);
    }
    sem_t *sem_p2_cons_enc2 = sem_open(SEM_CONSUMER_P2, O_CREAT | O_EXCL, 0660, 0);
    if (sem_p2_cons_enc2 == SEM_FAILED)
    {
        perror("sem_open/P2 consumer failed");
        exit(EXIT_FAILURE);
    }

    // Unlinking semaphore names before they are opened.
    sem_unlink(SEM_PRODUCER_P2);
    sem_unlink(SEM_CONSUMER_ENC2);

    // Semaphores for P2 to enc2 communication.
    sem_t *sem_enc2_cons_p2 = sem_open(SEM_CONSUMER_ENC2, O_CREAT | O_EXCL, 0660, 1);
    if (sem_enc2_cons_p2 == SEM_FAILED)
    {
        perror("sem_open/enc2 consumer failed");
        exit(EXIT_FAILURE);
    }
    sem_t *sem_p2_prod_enc2 = sem_open(SEM_PRODUCER_P2, O_CREAT | O_EXCL, 0660, 0);
    if (sem_p2_prod_enc2 == SEM_FAILED)
    {
        perror("sem_open/P2 producer failed");
        exit(EXIT_FAILURE);
    }

    // Unlinking semaphore names before they are opened.
    sem_unlink(SEM_ENC2_CONS);
    sem_unlink(SEM_CHANNEL_PROD2);

    // Fork for creating Enc2.
    if ((enc2_pid = fork()) == 0)
    {
        //------------------------------Enc2 proccess------------------------------//

        // Declaring all needed variables.
        char message[BLOCK_SIZE];
        char result[BLOCK_SIZE];
        char *check_sum;
        const char *receivedcheck_sum;

        // Here the information of the message is stored to see is if it is
        // equal to the TERMINATION_STRING.
        char *end_string = malloc(sizeof(char) * BLOCK_SIZE);

        // Semaphores for channel to enc2 communication.
        sem_t *sem_enc2_cons = sem_open(SEM_ENC2_CONS, O_CREAT | O_EXCL, 0660, 1);
        if (sem_enc2_cons == SEM_FAILED)
        {

            perror("sem_open/enc2 consumer failed");
            exit(EXIT_FAILURE);
        }
        sem_t *sem_channel_prod2 = sem_open(SEM_CHANNEL_PROD2, O_CREAT | O_EXCL, 0660, 0);
        if (sem_channel_prod2 == SEM_FAILED)
        {
            perror("sem_open/channel producer2 failed");
            exit(EXIT_FAILURE);
        }

        do
        {
            if (!P2Sender)
            {
                //-------------Enc2 when it receives a message from channel-------------//
                memset(message, 0, BLOCK_SIZE);
                memset(result, 0, BLOCK_SIZE);

                // Attaching shared memory block between Enc2 and channel so that the
                // message from the channel can be received.
                char *block_enc2_ch = attach_memory_block(BLOCK_ENC2_CH, BLOCK_SIZE);
                if (block_enc2_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Receiving the message from P1 through Enc1 and channel.
                sem_wait(sem_channel_prod2);
                strncpy(message, block_enc2_ch, BLOCK_SIZE);
                sem_post(sem_enc2_cons);
                detach_memory_block(block_enc2_ch);

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

                    // Semaphores for Enc2 to channel communication.
                    sem_t *sem_enc2_prod = sem_open(SEM_ENC2_PROD, 0);
                    if (sem_enc2_prod == SEM_FAILED)
                    {
                        perror("sem_open/enc2 producer failed");
                        exit(EXIT_FAILURE);
                    }
                    sem_t *sem_channel_cons2 = sem_open(SEM_CHANNEL_CONS2, 0);
                    if (sem_channel_cons2 == SEM_FAILED)
                    {
                        perror("sem_open/channel consumer2 failed");
                        exit(EXIT_FAILURE);
                    }

                    // Attaching shared memory block between Enc2 and channel.
                    char *block_enc2_ch = attach_memory_block(BLOCK_ENC2_CH, BLOCK_SIZE);
                    if (block_enc2_ch == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Sending the error value "0" back to P1 through channel and Enc1
                    // so that P1 can know that the message that was transmited changed
                    // in the channel and the message will have to be resent by P1.
                    sem_wait(sem_channel_cons2);
                    strncpy(block_enc2_ch, "0", BLOCK_SIZE);
                    sem_post(sem_enc2_prod);
                    detach_memory_block(block_enc2_ch);

                    // Attaching shared memory block between Enc2 and channel
                    // so that Enc2 can receive the resent message from P1.
                    block_enc2_ch = attach_memory_block(BLOCK_ENC2_CH, BLOCK_SIZE);
                    if (block_enc2_ch == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Receiving the resent message from P1.
                    sem_wait(sem_channel_prod2);
                    strncpy(message, block_enc2_ch, BLOCK_SIZE);
                    sem_post(sem_enc2_cons);
                    detach_memory_block(block_enc2_ch);

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

                    // Extracting the check sum from the message.
                    receivedcheck_sum = &message[i + 1];

                    // Calculating the new check sum of the received message.
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

                // Semaphores for Enc2 to channel communication.
                sem_t *sem_enc2_prod = sem_open(SEM_ENC2_PROD, 0);
                if (sem_enc2_prod == SEM_FAILED)
                {
                    perror("sem_open/enc2 producer failed");
                    exit(EXIT_FAILURE);
                }
                sem_t *sem_channel_cons2 = sem_open(SEM_CHANNEL_CONS2, 0);
                if (sem_channel_cons2 == SEM_FAILED)
                {
                    perror("sem_open/channel consumer2 failed");
                    exit(EXIT_FAILURE);
                }

                // Attaching shared memory block between channel and Enc2.
                block_enc2_ch = attach_memory_block(BLOCK_ENC2_CH, BLOCK_SIZE);
                if (block_enc2_ch == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // After the end of the previous loop the message has been correctly
                // transmitted and a check value of "1" is sent back to all channel,
                // Enc1 and P1 so that they can continue.
                sem_wait(sem_channel_cons2);
                strncpy(block_enc2_ch, "1", BLOCK_SIZE);
                sem_post(sem_enc2_prod);
                detach_memory_block(block_enc2_ch);

                // Copying the information that I extracted to the message
                // so that it can be sent to P2.
                memset(message, 0, BLOCK_SIZE);
                strcpy(message, result);

                // Attaching shared memory block between Enc2 and P2 so that the message
                // can me sent to P2.
                char *block_p2_enc2 = attach_memory_block(BLOCK_P2_ENC2, BLOCK_SIZE);
                if (block_p2_enc2 == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Sending final correct message from Enc2 to P2.
                sem_wait(sem_enc2_prod_p2);
                strncpy(block_p2_enc2, message, BLOCK_SIZE);
                sem_post(sem_p2_cons_enc2);
                detach_memory_block(block_p2_enc2);

                // Allocating end_string so that if it is equal to (TERMINATION_STRING)
                // the proccess will end.
                free(end_string);
                end_string = malloc(sizeof(char) * BLOCK_SIZE);
                strcpy(end_string, message);

                // Next P2 is the sender and P1 the receiver.
                P2Sender = true;
            }
            else
            {
                //-------------Enc2 when it sends a message to channel-------------//

                // Check stores the check value to see if the message was sent unchanged
                char check[2];
                do
                {
                    memset(message, 0, BLOCK_SIZE);

                    // Attaching shared memory block between P2 and Enc2 so that Enc2
                    // can receive the message from P2.
                    char *block_p2_enc2 = attach_memory_block(BLOCK_P2_ENC2, BLOCK_SIZE);
                    if (block_p2_enc2 == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Receiving the message from P2.
                    sem_wait(sem_p2_prod_enc2);
                    strncpy(message, block_p2_enc2, BLOCK_SIZE);
                    sem_post(sem_enc2_cons_p2);
                    detach_memory_block(block_p2_enc2);

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
                    //End of checksum calculation

                    // Semaphores for Enc2 to channel communication.
                    sem_t *sem_enc2_prod = sem_open(SEM_ENC2_PROD, 0);
                    if (sem_enc2_prod == SEM_FAILED)
                    {
                        perror("sem_open/enc2 producer failed");
                        exit(EXIT_FAILURE);
                    }
                    sem_t *sem_channel_cons2 = sem_open(SEM_CHANNEL_CONS2, 0);
                    if (sem_channel_cons2 == SEM_FAILED)
                    {
                        perror("sem_open/channel consumer2 failed");
                        exit(EXIT_FAILURE);
                    }

                    // Attaching shared memory block between Enc2 and channel so that
                    // the message along with the check sum can be sent to channel.
                    char *block_enc2_ch = attach_memory_block(BLOCK_ENC2_CH, BLOCK_SIZE);
                    if (block_enc2_ch == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Sending the message to channel.
                    sem_wait(sem_channel_cons2);
                    strncpy(block_enc2_ch, message, BLOCK_SIZE);
                    sem_post(sem_enc2_prod);
                    detach_memory_block(block_enc2_ch);

                    // Attaching shared memory block between Enc2 and channel so that Enc2 can
                    // receive the check value to see if the message was sent unchanged.
                    block_enc2_ch = attach_memory_block(BLOCK_ENC2_CH, BLOCK_SIZE);
                    if (block_enc2_ch == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Receiving check value from channel.
                    sem_wait(sem_channel_prod2);
                    strncpy(check, block_enc2_ch, BLOCK_SIZE);
                    sem_post(sem_enc2_cons);
                    detach_memory_block(block_enc2_ch);

                    // Attaching shared memory block between Enc2 and P2 so that Enc2 can send
                    // the check value to P2.
                    block_p2_enc2 = attach_memory_block(BLOCK_P2_ENC2, BLOCK_SIZE);
                    if (block_p2_enc2 == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Sending the check value to P2.
                    sem_wait(sem_enc2_prod_p2);
                    strncpy(block_p2_enc2, check, BLOCK_SIZE);
                    sem_post(sem_p2_cons_enc2);
                    detach_memory_block(block_p2_enc2);

                    // If the check value is "1" Enc2 will be allowed to continue otherwise
                    // it will continue receiving the message from P2 and sending it to Enc1
                    // through the channel until it is sent unchanged.
                } while (check[0] == '0');

                // Next P2 is the receiver and P1 is the sender.
                P2Sender = false;
            }

            // Checking if the message sent is the (TERMINATION_STRING).
        } while (strcmp(end_string, TERMINATION_STRING));

        // Message that Enc2 is ending.
        printf("Proccess Enc2 ending\n");

        // Closing all allocated semaphores and unlinking their names.
        sem_close(sem_channel_prod2);
        sem_close(sem_enc2_cons);
        sem_unlink(SEM_CHANNEL_PROD2);
        sem_unlink(SEM_ENC2_CONS);

        // Freeing the allocated memory for end string
        free(end_string);
    }
    else
    {
        //------------------------------P2 proccess------------------------------//

        // Declaring needed variables.
        char message[BLOCK_SIZE];

        // Here the information of the message is stored to see is if it is
        // equal to the TERMINATION_STRING.
        char *end_string = malloc(sizeof(char) * BLOCK_SIZE);
        do
        {
            // Check stores the check value to see if the message was sent unchanged.
            char check[2];
            if (!P2Sender)
            {
                //--------------P2 when it receives a message from P1--------------//

                memset(message, 0, BLOCK_SIZE);

                // Attaching shared memory block between P2 and Enc2 so that
                // the message can be received from Enc2 by P2.
                char *block_p2_enc2 = attach_memory_block(BLOCK_P2_ENC2, BLOCK_SIZE);
                if (block_p2_enc2 == NULL)
                {
                    printf("ERROR: Can't get block\n");
                    exit(EXIT_FAILURE);
                }

                // Receiving message from Enc2.
                sem_wait(sem_p2_cons_enc2);
                strncpy(message, block_p2_enc2, BLOCK_SIZE);
                sem_post(sem_enc2_prod_p2);
                detach_memory_block(block_p2_enc2);

                // Printing received message from P1.
                printf("Message from user P1:%s\n", message);

                // Next P2 is the sender and P1 the receiver.
                P2Sender = true;

                // Allocating end_string so that if it is equal to (TERMINATION_STRING)
                // the proccess will end.
                free(end_string);
                end_string = malloc(sizeof(char) * BLOCK_SIZE);
                strcpy(end_string, message);
            }
            else
            {
                //--------------P2 when it sends a message to P1--------------//

                memset(message, 0, BLOCK_SIZE);

                // Reading the message from input.
                printf("User P2:");
                fgets(message, BLOCK_SIZE, stdin);
                strtok(message, "\n");
                do
                {
                    // Attaching shared memory block between P2 and Enc2.
                    // so that the message can be sent to Enc2.
                    char *block_p2_enc2 = attach_memory_block(BLOCK_P2_ENC2, BLOCK_SIZE);
                    if (block_p2_enc2 == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Sending the message to Enc2.
                    sem_wait(sem_enc2_cons_p2);
                    strncpy(block_p2_enc2, message, BLOCK_SIZE);
                    sem_post(sem_p2_prod_enc2);
                    detach_memory_block(block_p2_enc2);

                    // Attaching shared memory block between P2 and Enc2.
                    // so that the check value can be received.
                    block_p2_enc2 = attach_memory_block(BLOCK_P2_ENC2, BLOCK_SIZE);
                    if (block_p2_enc2 == NULL)
                    {
                        printf("ERROR: Can't get block\n");
                        exit(EXIT_FAILURE);
                    }

                    // Receiving the check value from Enc2.
                    sem_wait(sem_p2_cons_enc2);
                    strncpy(check, block_p2_enc2, BLOCK_SIZE);
                    sem_post(sem_enc2_prod_p2);
                    detach_memory_block(block_p2_enc2);

                    // If the check value is "1" the proccess can continue otherwise it will
                    // resend the message to Enc2 so that it can be transmitted to P1.
                    // This will continue until the check value is "1" that is until the
                    // message is sent unchanged by the channel.
                } while (check[0] == '0');

                // Next P2 is the receiver and P1 is the sender.
                P2Sender = false;

                // Allocating end_string so that if it is equal to (TERMINATION_STRING)
                // the proccess will end.
                free(end_string);
                end_string = malloc(sizeof(char) * BLOCK_SIZE);
                strcpy(end_string, message);
            }

            // Checking if the message sent is the (TERMINATION_STRING).
        } while (strcmp(end_string, TERMINATION_STRING));

        // Printing that P2 is ending.
        printf("P2 proccess ending\n");
    }

    // Destroying al allocated shared memory blocks.
    destroy_memory_block(BLOCK_P2_ENC2);
    destroy_memory_block(BLOCK_ENC2_CH);

    // Closing all semaphores and unlinking their names.
    sem_close(sem_enc2_cons_p2);
    sem_close(sem_enc2_prod_p2);
    sem_close(sem_p2_cons_enc2);
    sem_close(sem_p2_prod_enc2);

    sem_unlink(SEM_PRODUCER_ENC2);
    sem_unlink(SEM_CONSUMER_P2);
    sem_unlink(SEM_PRODUCER_P2);
    sem_unlink(SEM_CONSUMER_ENC2);

    return 0;
}