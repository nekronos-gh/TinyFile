#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <mqueue.h>

#include "service.h"

#define MAX_MSG_SIZE 1024

#define INIT 0
#define REQUEST 1

#define SECTION 0
#define WRITE_OK 1
#define COMPRESS_OK 2

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
	*sms_size = 4096;
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

void init_shared_memory(size_t num_seg, size_t seg_size, int *segments) {
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

        // Set the size of the shared memory object
        if (ftruncate(shm_fd, seg_size) == -1) {
            perror("ftruncate");
            close(shm_fd);
            exit(EXIT_FAILURE);
        }

        // Close the file descriptor as it's no longer needed after mapping
		segments[i] = shm_fd;
    }

}

void close_shared_memory(size_t num_seg, int* segments) {
	for (size_t i = 0; i < num_seg; ++i) {
		close(segments[i]);
	}
}


void start_compressing(size_t num_seg, size_t seg_size, int *segments){
	for (size_t i = 0; i < num_seg; ++i) {
		char shm_name[256];
		//
		// Map the shared memory object
		void* addr = mmap(NULL, seg_size, PROT_READ, MAP_SHARED, segments[i], 0);
		if (addr == MAP_FAILED) {
			perror("mmap");
			continue; // Skip this segment and try the next
		}

		// Read data from the shared memory object
		// For example purposes, just printing the first byte
		printf("Segment %zu, first byte: %c\n", i, *((char*)addr));

		// Unmap the shared memory object
		munmap(addr, seg_size);
	}
}

void handle_compress(unsigned int pid, size_t num_seg, size_t seg_size, int *segments) {
    
    // XXX: get best process
    // TODO: 
    // - Allocate shared memory
    key_t key;


    // Send memory key on individual compress q
    mqd_t compress_mq;
    struct mq_attr attr;
    attr.mq_flags = 0; 
    attr.mq_maxmsg = 10; 
    attr.mq_msgsize = sizeof(message_compress_t);
    attr.mq_curmsgs = 0;

    char queue_string[256];
    sprintf(queue_string, "/%d", pid);
    compress_mq = mq_open(queue_string, O_RDWR);
    if (compress_mq == (mqd_t) -1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }
    
	// Write information about chunks to process
    message_compress_t buffer;
    buffer.type = SECTION;
    buffer.chunks = num_seg;
	buffer.size = seg_size;
    if (mq_send(compress_mq, (char *) &buffer, sizeof(message_compress_t), 0) == -1) {
        perror("mq_send");
        exit(EXIT_FAILURE);
    }

    // TODO:
    // - Compress file
	start_compressing(num_seg, seg_size, segments);

    // Send finished response
    buffer.type = COMPRESS_OK;
    if (mq_send(compress_mq, (char *) &buffer, sizeof(message_compress_t), 0) == -1) {
        perror("mq_send");
        exit(EXIT_FAILURE);
    }

}



int main(int argc, char *argv[]) {

    size_t n_sms, sms_size;
    parse_args(argc, argv, &n_sms, &sms_size);

	int segments[n_sms];
	init_shared_memory(n_sms, sms_size, segments);

    mqd_t main_mq; 
    struct mq_attr attr;

    attr.mq_flags = 0; 
    // XXX: choose max size
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
		printf("Recieved MESSAGE (%d) from %d\n", buffer.type, buffer.content);

		// XXX: This shoud be refactored in neatly functions
		switch(buffer.type){
		 case INIT:
            if (!add_to_llist(&head, buffer.content)) {
                // XXX: handle errors?
            } 
			break;
		case REQUEST:
            handle_compress(buffer.content, n_sms, sms_size, segments);
			break;
		default:
			printf("Message %d not understood\n", buffer.type);
		}
    }
    
	close_shared_memory(n_sms, segments);

	mq_close(main_mq);
	mq_unlink(TINY_FILE_QUEUE);
    return 0;
}
