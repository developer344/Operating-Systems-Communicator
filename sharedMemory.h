#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdbool.h>

// If a user inputs this string all proccesses end.
#define TERMINATION_STRING "TERM"

// Function for the managment of the shared memory.
char *attach_memory_block(char *filename, int size);
bool detach_memory_block(char *block);
bool destroy_memory_block(char *filename);

// The size of the shared memory block.
#define BLOCK_SIZE 4096

//Shared memory block names.
#define BLOCK_ENC1_CH "channel.c"
#define BLOCK_ENC2_CH "sharedMemory.c"
#define BLOCK_P1_ENC1 "P1.c"
#define BLOCK_P2_ENC2 "P2.c"

//-------------Semaphore Names-------------//

// P1 Enc1 relation.
#define SEM_PRODUCER_P1 "semP1ProducerToEnc1"
#define SEM_CONSUMER_P1 "semP1ConsumerFromP1"
#define SEM_PRODUCER_ENC1 "semEnc1ProducerToP1"
#define SEM_CONSUMER_ENC1 "semEnc1ConsumerFromEnc1"
// P2 Enc2 relation.
#define SEM_PRODUCER_P2 "semP2ProducerToEnc2"
#define SEM_CONSUMER_P2 "semP2ConsumerFromP2"
#define SEM_PRODUCER_ENC2 "semEnc2ProducerToP2"
#define SEM_CONSUMER_ENC2 "semEnc2ConsumerFromEnc2"
// Enc1 Channel relation.
#define SEM_ENC1_PROD "semEnc1ProducerToChannel"
#define SEM_ENC1_CONS "semEnc1ConsumerFromChannel"
#define SEM_CHANNEL_PROD1 "semChannelProducerToEnc1"
#define SEM_CHANNEL_CONS1 "semChannelConsumer1FromEnc1"
// Enc2 Channnel relation.
#define SEM_ENC2_PROD "semEnc2ProducerToChannel"
#define SEM_ENC2_CONS "semEnc2ConsumerFromChannel"
#define SEM_CHANNEL_PROD2 "semChannelProducer2ToEnc2"
#define SEM_CHANNEL_CONS2 "semChannelConsumer2FromEnc2"

#endif