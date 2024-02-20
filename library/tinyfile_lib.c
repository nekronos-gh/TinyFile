#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MAX_MSG_SIZE 1024

typedef struct msgbuf {
    long mtype;
    char mtext[MAX_MSG_SIZE];
} message_buf;

void send_message(const char *message) {

    printf("LIB: Call received\n");
    int msqid;
    key_t key = 42;  
    message_buf sbuf;

    // Connect to the message queue
    if ((msqid = msgget(key, 0666)) < 0) {
        perror("msgget");
        exit(1);
    }

    // Prepare the message
    sbuf.mtype = 1;  // Message type must be > 0
    strncpy(sbuf.mtext, message, MAX_MSG_SIZE);

    // Send the message
    if (msgsnd(msqid, &sbuf, strlen(sbuf.mtext) + 1, IPC_NOWAIT) < 0) {
        perror("msgsnd");
        exit(1);
    }
} 
