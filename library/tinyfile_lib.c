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
#define DEBUG 1
const unsigned int MUTEX_SIZE = sizeof(pthread_mutex_t);
const unsigned int COND_SIZE = sizeof(pthread_cond_t);
const unsigned int INFO_SIZE = sizeof(unsigned int); 
const unsigned int META_DATA_SIZE = MUTEX_SIZE + COND_SIZE + 2 * INFO_SIZE;

const unsigned int MUTEX_OFFSET     = 0;
const unsigned int COND_OFFSET      = MUTEX_SIZE;
const unsigned int STATUS_OFFSET    = MUTEX_SIZE    + COND_SIZE;
const unsigned int SIZE_OFFSET      = STATUS_OFFSET + INFO_SIZE;



// Start the communication with the Daemon
int init_communication(mqd_t *my_queue, mqd_t *tf_queue){
    printf("* Init communication START\n");
	message_main_t message;
    if (DEBUG) printf("* INIT start\n");
	// Open Deamon queue
	*tf_queue = mq_open(TINY_FILE_QUEUE, O_WRONLY);
    if (*tf_queue == (mqd_t)-1) {
        perror("Opening main mesq");
		return -1;
    }
    // Create a Hello message
	message.type = INIT;
	message.content = getpid();
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

    if (DEBUG) printf("* INIT end\n");
	return 0;
}

int close_communication(mqd_t my_queue, mqd_t tf_queue){

    if (DEBUG) printf("* CLOSE start\n");

    message_main_t message;
	message.type = CLOSE;
	message.content = getpid();
    if (mq_send(tf_queue, (char *) &message, sizeof(message), 0) == -1) {
        perror("mq_send");
		return -1;
    }
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
    if (DEBUG) printf("* CLOSE end\n");
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

const char* print_status(int code) {
    switch(code) {
        case EMPTY:
            return "EMPTY";
        case RAW:
            return "RAW";
        case COMPRESSED:
            return "COMPRESSED";
        case DONE_LIB:
            return "DONE_LIB";
        case DONE_SER:
            return "DONE_SER";
        default:
            return "Error";
    }
}

void print_memory(const void* ptr, size_t size) {
    const unsigned char* byte = (const unsigned char*) ptr;
    for (size_t i = 0; i < size; i++) {
        printf("%02x ", byte[i]);
        if ((i + 1) % 16 == 0) // Optional: line break every 16 bytes
            printf("\n");
    }
    printf("\n");
}

int* open_shared_memory(int n_chunks){
	int* result_fd = malloc(sizeof(int) * n_chunks);
	for (int i = 0; i < n_chunks; i++){
		char shm_name[256];
		snprintf(shm_name, sizeof(shm_name), "/tf_mem%d", i);

		result_fd[i] = shm_open(shm_name, O_RDWR, S_IRUSR | S_IWUSR);

		if (result_fd[i] == -1) {
			perror("shm_open");
			exit(EXIT_FAILURE);
		}
	}
	return result_fd;
}

void close_shared_memory(int n_chunks, int* fd_array){
	for (int i = 0; i < n_chunks; i++){
		close(fd_array[i]);
	}
	free(fd_array);
}

int not_done_copying_loop(FILE* file_in, FILE* file_out, int n_chunks, int* chunks, int chunk_data_size, long file_size){
     
    if (DEBUG) printf("* NOT DONE start\n");
	
    int i = 0;
	int done = 0;
	while (!done) {
		int idx = i % n_chunks;
		size_t total_size = META_DATA_SIZE + chunk_data_size;

		// Map the shared memory
		void* chunk_ptr = mmap(NULL, total_size, PROT_READ|PROT_WRITE, MAP_SHARED, chunks[idx], 0);
		if (chunk_ptr == MAP_FAILED) {
			perror("mmap");
            break;
		}
		// Get addresses
		pthread_mutex_t* mutex_ptr = (pthread_mutex_t*)((char*)chunk_ptr+ MUTEX_OFFSET);
		pthread_cond_t* cond_ptr = (pthread_cond_t*)((char*)chunk_ptr + COND_OFFSET);
		unsigned int* status_ptr = (unsigned int*)((char*)chunk_ptr + STATUS_OFFSET);
		unsigned int* size_ptr = (unsigned int*)((char*)chunk_ptr + SIZE_OFFSET);
        void* data_ptr = (char*)(chunk_ptr + META_DATA_SIZE);

		// Lock mutex
		pthread_mutex_lock(mutex_ptr);

		// Wait until compressed or empty 
		while (*status_ptr == RAW) {
            if (DEBUG) printf("In mutex (i=%d)(status=%d)\n", idx, *status_ptr);
			pthread_cond_wait(cond_ptr, mutex_ptr);
		}
        printf("\n -> chunk: %d (%s)\n", idx, print_status(*status_ptr));
		// If compressed read chunk into ouput file
		// Else if not empy error
		if (*status_ptr == COMPRESSED){
            if (DEBUG) {
                printf("Reading compressed bytes: ");
                print_memory(chunk_ptr + META_DATA_SIZE, *size_ptr);
            }
			fwrite(data_ptr, 1, *size_ptr, file_out);
		} else if (*status_ptr != EMPTY){
			pthread_mutex_unlock(mutex_ptr);
			pthread_cond_signal(cond_ptr);
			munmap(chunk_ptr, total_size);
			exit(EXIT_FAILURE);
		}

		// IF compressed or empy, write data into chunk
		size_t read = fread(data_ptr, 1, chunk_data_size, file_in);
		*size_ptr = read;
		if (read < chunk_data_size) {
			*status_ptr = DONE_LIB;
			done = 1;
		} else {
			*status_ptr = RAW;
		}
        if (DEBUG) {
            printf("Writing raw bytes: ");
            print_memory(chunk_ptr + META_DATA_SIZE, *size_ptr);
        }
        printf(" <- %s\n", print_status(*status_ptr));
		pthread_mutex_unlock(mutex_ptr);
		pthread_cond_signal(cond_ptr);
		munmap(chunk_ptr, total_size);
		i++;
	}
    if (DEBUG) printf("* NOT DONE end\n");
    return i;
}

void done_copying_loop(FILE* file_in, FILE* file_out, int n_chunks, int* chunks, int chunk_data_size, int file_size, int next_chunk){
    
    if (DEBUG) printf("* DONE start\n");
	int i = next_chunk;
	int done = 0;
    while (!done) {
        int idx = i % n_chunks;
		size_t total_size = META_DATA_SIZE + chunk_data_size;

		// Map the shared memory
        void* chunk_ptr = mmap(NULL, total_size, PROT_READ|PROT_WRITE, MAP_SHARED, chunks[idx], 0);
		if (chunk_ptr == MAP_FAILED) {
			perror("mmap");
			continue; // Skip this segment and try the next
		}

		// Get addresses
        pthread_mutex_t* mutex_ptr = (pthread_mutex_t*)((char*)chunk_ptr+ MUTEX_OFFSET);
        pthread_cond_t* cond_ptr = (pthread_cond_t*)((char*)chunk_ptr + COND_OFFSET);
        unsigned int* status_ptr = (unsigned int*)((char*)chunk_ptr + STATUS_OFFSET);
        unsigned int* size_ptr = (unsigned int*)((char*)chunk_ptr + SIZE_OFFSET);
        void* data_ptr = (char*)(chunk_ptr + META_DATA_SIZE);

        // Lock mutex
        pthread_mutex_lock(mutex_ptr);

        // Wait until compressed or empty 
        while (*status_ptr == RAW || *status_ptr == DONE_LIB) {
            pthread_cond_wait(cond_ptr, mutex_ptr);
        }
        printf("\n -> chunk: %d (%s)\n", idx, print_status(*status_ptr));

		if (*status_ptr != EMPTY) {
            if (DEBUG) {
                printf("Reading compressed bytes: ");
                print_memory(chunk_ptr + META_DATA_SIZE, *size_ptr);
            }
			fwrite(data_ptr, 1, *size_ptr, file_out);
		}

		if (*status_ptr == DONE_SER){
			done = 1;
		}
        printf(" <- %s\n", print_status(*status_ptr));
		pthread_mutex_unlock(mutex_ptr);
		pthread_cond_signal(cond_ptr);
		munmap(chunk_ptr, total_size);
		i++;
    }
    if (DEBUG) printf("* DONE end\n");
}

/// Ring buffer communication with service
/// Write/read until receive whole compressed file
int get_compressed_file(FILE* file_in, FILE* file_out, int n_chunks, int* chunks, int chunk_data_size) {
    
    // Get message size
    // XXX: can be computed once and passed as argument
    long file_size = get_file_size(file_in);
	int next_chunk = not_done_copying_loop(file_in, file_out, n_chunks, chunks, chunk_data_size, file_size);
	done_copying_loop(file_in, file_out, n_chunks, chunks, chunk_data_size, file_size, next_chunk);
    
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

    // Send START message
	message_main.type = REQUEST;
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

	int* chunks = open_shared_memory(n_chunks);
    if (get_compressed_file(file_in, file_out, n_chunks, chunks, size) < 0) {
        // XXX: ??
    }
   
    // Send message for finished reading
    message_compress.type = LIB_FINISHED;
    if (mq_send(my_queue, (char *) &message_compress, sizeof(message_compress), 0) == -1) {
        perror("mq_send");
		return -1;
    }
	close_shared_memory(n_chunks, chunks);
    fclose(file_out);
    fclose(file_in);

	return 0;
}

