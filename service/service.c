#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <getopt.h>
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
    while ((opt = getopt_long(argc, argv, "n:s:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'n':
                *n_sms = atoi(optarg);
                break;
            case 's':
                *sms_size = atoi(optarg);
                break;
            default: 
                fprintf(stderr, "Usage: %s --n_sms <num_segments> --sms_size <size_in_bytes>\n", argv[0]);
                exit(1);
        }
    }
}

void handle_compress(unsigned int pid) {
    
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

    char* queue_string;
    sprintf(queue_string, "/%d", pid);
    compress_mq = mq_open(queue_string, O_RDWR);
    if (compress_mq == (mqd_t) -1) {
        perror("mq_open");
        exit(1);
    }
    
	#define TODO_ERASE_THIS key = 10
	TODO_ERASE_THIS;

    message_compress_t buffer;
    buffer.type = SECTION;
    buffer.content = key;
    if (mq_send(compress_mq, (char *) &buffer, sizeof(message_compress_t), 0) == -1) {
        perror("mq_send");
        exit(1);
    }

    // Wait for response on ind compress q
    ssize_t bytes_read;
    bytes_read = mq_receive(compress_mq, (char *)&buffer, sizeof(message_compress_t), NULL);
    if (bytes_read < 0) {
        perror("mq_receive");
        exit(1);
    }

    if (!(buffer.type == WRITE_OK)) {
        // XXX: handle errors
    }

    // TODO:
    // - Compress file

    // Send finished response
    buffer.type = COMPRESS_OK;
    if (mq_send(compress_mq, (char *) &buffer, sizeof(message_compress_t), 0) == -1) {
        perror("mq_send");
        exit(1);
    }

}



int main(int argc, char *argv[]) {

    size_t n_sms, sms_size;
    parse_args(argc, argv, &n_sms, &sms_size);

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
        exit(1);
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
            exit(1);
        }
		printf("Recieved MESSAGE (%d) from %d\n", buffer.type, buffer.content);

		switch(buffer.type){
		 case INIT:
            if (!add_to_llist(&head, buffer.content)) {
                // XXX: handle errors?
            } 
			break;
		case REQUEST:
            handle_compress(buffer.content);
			break;
		default:
			printf("Message %d not understood\n", buffer.type);
		}
    }
    
	mq_close(main_mq);
	mq_unlink(TINY_FILE_QUEUE);
    return 0;
}
