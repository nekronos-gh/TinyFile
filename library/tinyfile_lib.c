#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

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
    attr.mq_msgsize = sizeof(message_compress_t); 	// Maximum message size
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

/// Map all shared memory chunks
/// Initialize mutexes and conditionals
void init_shared_memory(chunk_meta_t** chunks, int n_chunks, int size) {
    for (int i = 0; i < n_chunks; i++) {
        char shm_name[256];
        snprintf(shm_name, sizeof(shm_name), "/tf_mem%d", i);

        // Open the shared memory object
        int fd = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            perror("shm_open");
            exit(EXIT_FAILURE);
        }

        // Size = metadata + data size
        size_t total_size = sizeof(chunk_meta_t) + size;

        // Resize the shared memory
        if (ftruncate(fd, total_size) == -1) {
            perror("ftruncate");
            exit(EXIT_FAILURE);
        }

        // Map the shared memory object
        void* addr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED) {
            perror("mmap");
            exit(EXIT_FAILURE);
        }

        // Save pointer
        chunks[i] = (chunk_meta_t*)addr;
        pthread_mutexattr_t mutex_attr;
        pthread_condattr_t cond_attr;
        
        // Init mutex
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&chunks[i]->mutex, &mutex_attr);

        // Init cond
        pthread_condattr_init(&cond_attr);
        pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(&chunks[i]->cond, &cond_attr);

        // Clean up
        pthread_mutexattr_destroy(&mutex_attr);
        pthread_condattr_destroy(&cond_attr);
        close(fd);
    }
}

/// Ring buffer communication with service
/// Write/read until receive whole compressed file
int get_compressed_file(FILE* file_in, FILE* file_out, int n_chunks, int chunk_data_size) {
    
    // Init mem
    chunk_meta_t* chunks[n_chunks];
    init_shared_memory(chunks, n_chunks, chunk_data_size);

    // Get message size
    // XXX: can be computed once and passed as argument
    long file_size = get_file_size(file_in);

    // Loops required
    int loops = (file_size / chunk_data_size) + (file_size % chunk_data_size != 0);

    for (int i=0; i<loops; ++i) {
        int idx = i % n_chunks;

        // Lock mutex
        pthread_mutex_lock(&(chunks[idx]->mutex));

        // Wait until the chunk has compressed bytes or is empty
        while (chunks[idx]->status == RAW) {
            pthread_cond_wait(&(chunks[idx]->cond), &(chunks[idx]->mutex));
        }

        // If compressed content, read before writing
        if (chunks[idx]->status == COMPRESSED) {
            // TODO: read compressed
            // compressed bytes chunks[idx]->size
            // write to file_out
        }
        // TODO: write raw bytes

        // Update metadata
        // chunks[idx]->size = bytes written
        chunks[idx]->status = RAW;
        
        pthread_cond_signal(&(chunks[idx]->cond));
        pthread_mutex_unlock(&(chunks[idx]->mutex));
    }

    // TODO: write file
    return 0;
}
int compress_file(mqd_t my_queue, mqd_t tf_queue, const char *path_in, const char *path_out){
    
    // Open file to compress
	FILE *file_in = fopen(path_in, "rb");
	if (!file_in) {
		perror("Could not open input file");
		return -1;
	}
    // Check that path out
    FILE *file_out = fopen(path_out, "wb");
    if (!file_out) {
        perror("Could not open output file");
        return -1; 
    }

    // Messages for main and individual mesq
	message_main_t message_main;
	message_compress_t message_compress;

	printf("Now sending start message\n");
    // Send START message
	message_main.type = COMPRESS_REQUEST;
	message_main.content = getpid();
    // XXX: include file size?
    if (mq_send(tf_queue, (char *) &message_main, sizeof(message_main), 0) == -1) {
        perror("mq_send");
		return -1;
    }

	// Wait for response in individual mesq
	if (mq_receive(my_queue, (char *) &message_compress,  sizeof(message_compress), NULL) == -1){
        perror("mq_receive");
		return -1;
	}
    unsigned int n_chunks = message_compress.chunks;
    unsigned int size = message_compress.size;
    if (message_compress.type == COMPRESS_START) {
        if (get_compressed_file(file_in, file_out, n_chunks, size) < 0) {
            // TODO: error handling
        }
    }
    else {
        // TODO: logging
    }

    /*
	FILE *file_out = fopen(path_in, "w");
	if (!file_out) {
		perror("Could not open output file");
	}
    */

	return 0;
}

