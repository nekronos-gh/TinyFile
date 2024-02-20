#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <getopt.h>

#define MAX_MSG_SIZE 1024

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

int main(int argc, char *argv[]) {
    int msqid;
    key_t key = 42;  
    message_buf rbuf;

	size_t n_sms, sms_size;
	parse_args(argc, argv, &n_sms, &sms_size);

    // Set up the message queue
    if ((msqid = msgget(key, IPC_CREAT | 0666)) < 0) {
        perror("msgget");
        exit(1);
    }

    printf("SERVICE: (waiting) ...\n");

    while (1) {
        if (msgrcv(msqid, &rbuf, sizeof(rbuf.mtext), 0, 0) < 0) {
            perror("msgrcv");
            exit(1);
        }
        printf("SERVICE: Received --> %s\n", rbuf.mtext);
    }

    // Cleanup (in practice, you'd need a signal handler for graceful shutdown)
    msgctl(msqid, IPC_RMID, NULL);

    return 0;
}
