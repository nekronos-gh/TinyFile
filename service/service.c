#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <mqueue.h>

#include "service.h"

#define MAX_MSG_SIZE 1024
const unsigned int MUTEX_SIZE = sizeof(pthread_mutex_t);
const unsigned int COND_SIZE = sizeof(pthread_cond_t);
const unsigned int INFO_SIZE = sizeof(unsigned int); 
const unsigned int META_DATA_SIZE = MUTEX_SIZE + COND_SIZE + 2 * INFO_SIZE;

const unsigned int MUTEX_OFFSET = 0;
const unsigned int COND_OFFSET = MUTEX_SIZE;
const unsigned int STATUS_OFFSET = MUTEX_SIZE + COND_SIZE;
const unsigned int SIZE_OFFSET = STATUS_OFFSET + INFO_SIZE;



typedef struct msgbuf {
    long mtype;
    char mtext[MAX_MSG_SIZE];
} message_buf;

void parse_args(int argc, char *argv[], size_t *n_sms, size_t *sms_size) {
    int opt;
    // Define long options
    static struct option long_options[] = {
        {"n_sms", required_argument, 0, 'n'},
        {"sms_size", required_argument, 0, 's'},
        {0, 0, 0, 0} 
    };

    // Parse the options
	// Default values
	*n_sms = 5;
	*sms_size = 10;
    while ((opt = getopt_long(argc, argv, "n:s:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'n':
                *n_sms = atoi(optarg);
				if (*n_sms > 999 || *n_sms < 0){
					fprintf(stderr, "usage: n_sms must be greater than 0 or less than 999\n");
					exit(EXIT_FAILURE);
				}
                break;
            case 's':
                *sms_size = atoi(optarg);
				if (*sms_size > 999 || *sms_size < 0){
					fprintf(stderr, "usage: n_sms must be greater than 0 or less than 999\n");
					exit(EXIT_FAILURE);
				}
                break;
            default: 
                fprintf(stderr, "usage: %s --n_sms <num_segments> --sms_size <size_in_bytes>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
}

void init_mutex(pthread_mutex_t* mutex) {
	pthread_mutexattr_t mutexAttr;
	pthread_mutexattr_init(&mutexAttr);
	pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(mutex, &mutexAttr);
}

void init_mutex_cond(pthread_cond_t* cond) {
	pthread_condattr_t condAttr;
	pthread_condattr_init(&condAttr);
	pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);
	pthread_cond_init(cond, &condAttr);
}

void init_shared_memory(size_t num_seg, size_t *seg_size, int *segments) {
	for (size_t i = 0; i < num_seg; ++i) {
        char shm_name[256];
        // Generate a unique name for each segment
        snprintf(shm_name, sizeof(shm_name), SHARED_MEMORY"%zu", i);

        // Create the shared memory object
        int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("shm_open");
            exit(EXIT_FAILURE);
        }
		// Setup for sincronization
		// All this data will be written into shared memory
		pthread_mutex_t new_mutex;
		init_mutex(&new_mutex);
		pthread_cond_t new_cond;
		init_mutex_cond(&new_cond);
		unsigned int state = EMPTY;
		unsigned int data_size = 0;

        // Set the size of the shared memory object
		int total_seg_size = *seg_size + META_DATA_SIZE;
        if (ftruncate(shm_fd, total_seg_size) == -1) {
            perror("ftruncate");
            close(shm_fd);
            exit(EXIT_FAILURE);
        }

		// Write the setup into shared memory
		void* addr = mmap(NULL, total_seg_size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
		if (addr == MAP_FAILED) {
			perror("mmap");
			continue; // Skip this segment and try the next
		}

		memcpy((char*)addr,                 &new_mutex, MUTEX_SIZE);
		memcpy((char*)addr + COND_OFFSET,   &new_cond,  COND_SIZE);
		memcpy((char*)addr + STATUS_OFFSET, &state,     INFO_SIZE);
		memcpy((char*)addr + SIZE_OFFSET,   &data_size, INFO_SIZE);

		munmap(addr, total_seg_size);
		segments[i] = shm_fd;
    }

}

void close_shared_memory(size_t num_seg, int* segments) {
	for (size_t i = 0; i < num_seg; ++i) {
		close(segments[i]);
        char shm_name[256];
        // Generate a unique name for each segment
        snprintf(shm_name, sizeof(shm_name), SHARED_MEMORY"%zu", i);
		shm_unlink(shm_name);
	}
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

int start_compressing(size_t n_chunks, size_t chunk_data_size, int *chunks){

    int i = 0;
	int done = 0;
	while (!done) {
		int idx = i % n_chunks;
		
        // Map the shared memory object
		int total_seg_size = chunk_data_size + META_DATA_SIZE;
		void* chunk_ptr = mmap(NULL, total_seg_size, PROT_READ|PROT_WRITE, MAP_SHARED, chunks[idx], 0);
		if (chunk_ptr == MAP_FAILED) {
			perror("mmap");
            exit(EXIT_FAILURE);
		}

        pthread_mutex_t* mutex_ptr = (pthread_mutex_t*)((char*)chunk_ptr+ MUTEX_OFFSET);
        pthread_cond_t* cond_ptr = (pthread_cond_t*)((char*)chunk_ptr + COND_OFFSET);
        unsigned int* status_ptr = (unsigned int*)((char*)chunk_ptr + STATUS_OFFSET);
        unsigned int* size_ptr = (unsigned int*)((char*)chunk_ptr + SIZE_OFFSET);

		pthread_mutex_lock(mutex_ptr);

        while (*status_ptr != RAW && *status_ptr != DONE_LIB) {
            pthread_cond_wait(cond_ptr, mutex_ptr);
        }
        if (DEBUG) {
            printf("\n -> chunk: %d (%s)\n", idx, print_status(*status_ptr));
		    printf("Some random mf just wrote in segment %d:\n", idx);
    		print_memory(chunk_ptr + META_DATA_SIZE, *size_ptr);
        }

        // TODO: COMPRESSE
        *status_ptr = (*status_ptr == RAW) ? COMPRESSED : DONE_SER;
        done = (*status_ptr == DONE_SER);
		// Unmap the shared memory object
        if (DEBUG) printf(" <- chunk: %d (%s)\n", idx, print_status(*status_ptr));
		pthread_mutex_unlock(mutex_ptr);
		pthread_cond_signal(cond_ptr);
		munmap(chunk_ptr, total_seg_size);
        i++;
	}
    return i;
}


void handle_compress(unsigned int pid, size_t n_chunks, size_t chunk_size, int *chunks) {
    
    // XXX: get best process
    key_t key;


    // Send memory info on individual mesq
    mqd_t compress_mq;
    struct mq_attr attr;
    attr.mq_flags = 0; 
    attr.mq_maxmsg = 10; 
    attr.mq_msgsize = sizeof(message_compress_t);
    attr.mq_curmsgs = 0;

    char queue_string[256];
    sprintf(queue_string, "/%u", pid);
    compress_mq = mq_open(queue_string, O_RDWR);
    if (compress_mq == (mqd_t) -1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }
    
	// Write information about chunks to process
    message_compress_t buffer;
    buffer.type = MEMORY_INFO;
    buffer.chunks = n_chunks;
	buffer.size = chunk_size;
    if (mq_send(compress_mq, (char *) &buffer, sizeof(message_compress_t), 0) == -1) {
        perror("mq_send");
        exit(EXIT_FAILURE);
    }

	int reps = start_compressing(n_chunks, chunk_size, chunks);
    // Wait for done response
    ssize_t bytes_read;
    bytes_read = mq_receive(compress_mq, (char *) &buffer, sizeof(message_compress_t), NULL);
    if (bytes_read < 0) {
        exit(EXIT_FAILURE);
    }
    if (buffer.type != LIB_FINISHED) {
        // XXX: ??
    }

    // Clean status
    for (int i=0; i<n_chunks; i++) {
		
        // Map the shared memory object
		int total_seg_size = chunk_size + META_DATA_SIZE;
		void* chunk_ptr = mmap(NULL, total_seg_size, PROT_READ|PROT_WRITE, MAP_SHARED, chunks[i], 0);
		if (chunk_ptr == MAP_FAILED) {
			perror("mmap");
            exit(EXIT_FAILURE);
		}
        unsigned int* status_ptr = (unsigned int*)((char*)chunk_ptr + STATUS_OFFSET);
        *status_ptr = EMPTY;
	}
    if (!DEBUG) printf("* Compressed for %d DONE (%d)\n", pid, reps);

}



int main(int argc, char *argv[]) {

    size_t n_chunks, chunk_size;
    parse_args(argc, argv, &n_chunks, &chunk_size);

	int chunks[n_chunks];
	init_shared_memory(n_chunks, &chunk_size, chunks);

    mqd_t main_mq; 
    struct mq_attr attr;

    attr.mq_flags = 0; 
    attr.mq_maxmsg = 10;  // System max limit in: /proc/sys/fs/mqueue/msg_max
    attr.mq_msgsize = sizeof(message_main_t); 
    attr.mq_curmsgs = 0; 

    // Setup main q
    main_mq = mq_open(TINY_FILE_QUEUE, O_RDONLY | O_CREAT, 0644, &attr);
    if (main_mq == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    // Create linked list
    node_t* head = malloc(sizeof(node_t));

    ssize_t bytes_read;
    message_main_t buffer;
    while (1) {

        // Read main buffer
        bytes_read = mq_receive(main_mq, (char *) &buffer, sizeof(message_main_t), NULL);
        if (bytes_read < 0) {
            perror("mq_receive");
            exit(EXIT_FAILURE);
        }
		switch(buffer.type){
		 case INIT:
            if (add_to_llist(&head, buffer.content) < 0) {
                // XXX: ??
            } 
			break;

		case REQUEST:
            if (!DEBUG) printf("* Got request from %d\n", buffer.content);
            add_request(&head, buffer.content);
            unsigned int responding_to = get_request(&head);
            handle_compress(responding_to, n_chunks, chunk_size, chunks);
			break;

        case CLOSE:
            if (remove_from_llist(&head, buffer.content) < 0) {
                // XXX: ??
            }
            break;

		default:
			fprintf(stderr, "Message %d not understood\n", buffer.type);
            exit(EXIT_FAILURE);
		}
    }
    
	close_shared_memory(n_chunks, chunks);

	mq_close(main_mq);
	mq_unlink(TINY_FILE_QUEUE);
    return 0;
}
