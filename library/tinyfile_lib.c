#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>

#include "tinyfile_lib.h"

#define MAX_MSG_SIZE 1024
const unsigned int MUTEX_SIZE = sizeof(pthread_mutex_t);
const unsigned int COND_SIZE = sizeof(pthread_cond_t);
const unsigned int INFO_SIZE = sizeof(unsigned int); 
const unsigned int META_DATA_SIZE = MUTEX_SIZE + COND_SIZE + 2 * INFO_SIZE;
const unsigned int MUTEX_OFFSET = 0;
const unsigned int COND_OFFSET = MUTEX_SIZE;
const unsigned int STATUS_OFFSET = MUTEX_SIZE + COND_SIZE;
const unsigned int SIZE_OFFSET = STATUS_OFFSET + INFO_SIZE;


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
void init_shared_memory(void** chunks, int n_chunks, int chunk_data_size) {

    for (int i = 0; i < n_chunks; i++) {
        char shm_name[256];
        snprintf(shm_name, sizeof(shm_name), "/tf_mem%d", i);

        int fd = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            perror("shm_open");
            exit(EXIT_FAILURE);
        }

        size_t total_size = META_DATA_SIZE + chunk_data_size;
        if (ftruncate(fd, total_size) == -1) {
            perror("ftruncate");
            exit(EXIT_FAILURE);
        }

        void* addr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED) {
            perror("mmap");
            exit(EXIT_FAILURE);
        }

        // Direct casting to individual components
        pthread_mutex_t* mutex = (pthread_mutex_t*)addr;
        pthread_cond_t* cond = (pthread_cond_t*)((char*)addr + COND_OFFSET);

        // Init mutex and cond
        pthread_mutexattr_t mutex_attr;
        pthread_condattr_t cond_attr;

        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(mutex, &mutex_attr);

        pthread_condattr_init(&cond_attr);
        pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(cond, &cond_attr);

        pthread_mutexattr_destroy(&mutex_attr);
        pthread_condattr_destroy(&cond_attr);

        chunks[i] = addr;
        close(fd);
    }
}

/// Ring buffer communication with service
/// Write/read until receive whole compressed file
int get_compressed_file(FILE* file_in, FILE* file_out, int n_chunks, int chunk_data_size) {
    
    // Init mem
    void* chunks[n_chunks];
    init_shared_memory(chunks, n_chunks, chunk_data_size);

    // Get message size
    // XXX: can be computed once and passed as argument
    long file_size = get_file_size(file_in);
    int i = 0;
    int done_copying_raw = 0;
    while (1) {
        int idx = i % n_chunks;
        void* chunk_ptr = chunks[idx];
        pthread_mutex_t* mutex_ptr = (pthread_mutex_t*)((char*)chunk_ptr+ MUTEX_OFFSET);
        pthread_cond_t* cond_ptr = (pthread_cond_t*)((char*)chunk_ptr + COND_OFFSET);
        unsigned int* status_ptr = (unsigned int*)((char*)chunk_ptr + STATUS_OFFSET);
        unsigned int* size_ptr = (unsigned int*)((char*)chunk_ptr + SIZE_OFFSET);

        // Lock mutex
        pthread_mutex_lock(mutex_ptr);

        // Wait until compressed or empty 
        while (*status_ptr == RAW) {
            pthread_cond_wait(cond_ptr, mutex_ptr);
        }
        // If empty and finished copying, go to next block
        if (*status_ptr == EMPTY || done_copying_raw) {
            i++;
            continue;
        }

        void* data_ptr = (char*)(chunks[idx] + META_DATA_SIZE);

        // If compressed, write to file out
        if (*status_ptr == COMPRESSED) {
            fwrite(data_ptr, 1, *size_ptr, file_out);
        }

        if (*status_ptr == DONE) {
            // If both done, finished, SUCCESS
            if (done_copying_raw) {        
                pthread_cond_signal(cond_ptr);
                pthread_mutex_unlock(mutex_ptr);
                break;
            }
            else {
                // XXX: should never be here
            }
        }
        
        // If not done copying and not RAW, write RAW
        if (!done_copying_raw) {
            size_t read = fread(data_ptr, 1, chunk_data_size, file_in);
            *size_ptr = read;
            *status_ptr = (read < chunk_data_size) ? DONE : RAW;
            done_copying_raw = (read < chunk_data_size);
        }
        
        pthread_cond_signal(cond_ptr);
        pthread_mutex_unlock(mutex_ptr);
        i++;
    }
    
    fclose(file_out);
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
    if (get_compressed_file(file_in, file_out, n_chunks, size) < 0) {
        // TODO: error handling
    }

	return 0;
}

