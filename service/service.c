#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

#define MAX_MSG_SIZE 1024

typedef struct msgbuf {
    long mtype;
    char mtext[MAX_MSG_SIZE];
} message_buf;

int main(int argc, char *argv[]) {
    int msqid;
    key_t key = 42;  
    message_buf rbuf;

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
