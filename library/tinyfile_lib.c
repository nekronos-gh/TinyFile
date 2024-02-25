#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#include "tinyfile_lib.h"

#define MAX_MSG_SIZE 1024

// Start the communication with the Daemon
int init_communication(mqd_t *my_queue, mqd_t *tf_queue){
	message_main_t message;

	// Open Deamon queue
	*tf_queue = mq_open(TINY_FILE_QUEUE, O_WRONLY);
    if (*tf_queue == (mqd_t)-1) {
        perror("mq_open");
		return -1;
    }

    // Create a Hello message
	message.type = HELLO;
	message.content = getpid();
	printf("Now sending hello handshake\n");
    if (mq_send(*tf_queue, (char *) &message, sizeof(message), 0) == -1) {
        perror("mq_send");
		return -1;
    }

	char queue_name[64]; // Buffer to hold the queue name

    // Format the queue name with the prefix and PID
    sprintf(queue_name, "/%d", getpid());

    // Set the attributes of the message queue
    struct mq_attr attr;
    attr.mq_flags = 0; 		// Blocking queue
    attr.mq_maxmsg = 10; 	// Maximum number of messages in queue
    attr.mq_msgsize = sizeof(message_main_t); 	// Maximum message size
    attr.mq_curmsgs = 0; 	// Number of messages currently in queue

    // Create the message queue
    *my_queue = mq_open(queue_name, O_CREAT | O_RDWR, 0644, &attr);
    if (*my_queue == (mqd_t)-1) {
        perror("mq_open");
		return -1;
    }

	return 0;
}

int close_communication(mqd_t my_queue, mqd_t tf_queue){

	// Close daemon queue
	if (mq_close(tf_queue) == -1) {
        perror("mq_close");
		return -1;
    }

	// Close private queue
	if (mq_close(my_queue) == -1) {
        perror("mq_close");
		return -1;
    }

	// Remove message queue from system
	char queue_name[64];
    sprintf(queue_name, "/%d", getpid());
    if (mq_unlink(queue_name) == -1) {
        perror("mq_unlink");
		return -1;
    }
	return 0;
}

// Get the size of a fie
long get_file_size(FILE *file) {
    long original_position = ftell(file); // Save the current position.
    if (original_position == -1) return -1; // Error occurred

    // Seek to the end of the file.
    if (fseek(file, 0, SEEK_END) != 0) return -1; // Error occurred

    // Get the size of the file.
    long size = ftell(file);
    if (size == -1) return -1; // Error occurred

    // Restore the original position.
    if (fseek(file, original_position, SEEK_SET) != 0) return -1; // Error occurred

    return size;
}

int compress_file(mqd_t my_queue, mqd_t tf_queue, const char *path_in, const char *path_out){

	FILE *file_in = fopen(path_in, "rb");
	if (!file_in) {
		perror("Could not open input file");
		return -1;
	}

	message_main_t message_main;
	
	printf("Now sending start message\n");
    // Create a Start Compressing message
	message_main.type = COMPRESS_START;
	message_main.content = getpid();
    if (mq_send(tf_queue, (char *) &message_main, sizeof(message_main), 0) == -1) {
        perror("mq_send");
		return -1;
    }

	// Wait for free section message in queue
	message_compress_t message_compress;
	if (mq_receive(my_queue, (char *) &message_compress,  sizeof(message_compress), NULL) == -1){
        perror("mq_receive");
		return -1;
	}

	if (message_compress.type != SECTION){
        perror("Did not receive SECTION from daemon\n");
		return -1;
	}

	printf("Now placing uncompressed file in remote section %d \n", message_compress.content);

	// Send that uncompressed file has been written
	message_compress.type = WRITE_OK;
	message_compress.content = 0;
	if (mq_send(my_queue, (char *) &message_compress,  sizeof(message_compress), 0) == -1){
        perror("mq_receive");
		return -1;
	}

	// Wait form compression done 
	if (mq_receive(my_queue, (char *) &message_compress,  sizeof(message_compress), NULL) == -1){
		perror("mq_receive");
		return -1;
	}

	if (message_compress.type != COMPRESS_OK){
        perror("Did not receive COMPRESS_DONE from daemon\n");
		return -1;
	}

	printf("Now getting compressed result\n");

	FILE *file_out = fopen(path_in, "w");
	if (!file_out) {
		perror("Could not open output file");
	}

	return 0;
}

