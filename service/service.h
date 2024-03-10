#ifndef __SERVICE
#define __SERVICE

#include <stdio.h>
#include <stdlib.h>

#define DEBUG 1

#define TINY_FILE_QUEUE "/tinyservice"
#define SHARED_MEMORY "/tf_mem"

#define INIT 0x00
#define REQUEST 0x01
#define CLOSE 0x02
typedef struct message_main {
    unsigned int type;
	unsigned int content;
} message_main_t;


#define MEMORY_INFO 0x00
#define LIB_FINISHED 0x01
typedef struct message_compress {
    unsigned int type;
	unsigned int chunks;
    unsigned int size;
} message_compress_t;


#define EMPTY 0x00
#define RAW 0x01
#define COMPRESSED 0x02
#define DONE_LIB 0x03
#define DONE_SER 0x04


typedef struct node node_t;
typedef struct node {
    node_t* next;
    unsigned int pid;
    int n_request;
    int waiting;
} node_t;



/// Add new node to head
int add_to_llist(node_t** head, unsigned int pid) {
    // Allocate
    node_t* new_node = (node_t*)malloc(sizeof(node_t));
    if (new_node == NULL) {
        return -1;
    }

    // New node
    new_node->pid = pid;
    new_node->n_request = 0;
    new_node->waiting = 0;
    new_node->next = *head; 

    *head = new_node;
    return 0;
}


int remove_from_llist(node_t** head, unsigned int pid) {
    if (*head == NULL) {
        return -1;
    }
    node_t *temp = *head;
    node_t *prev = NULL;

    if (temp != NULL && temp->pid == pid) {
        *head = temp->next; 
        free(temp);
        return 0; 
    }

    while (temp != NULL && temp->pid != pid) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) return -1;

    prev->next = temp->next;

    free(temp);

    return 0;
}


int add_request(node_t** head, unsigned int pid) {
    if (*head == NULL) {
        return -1;
    }
    node_t* temp = *head;

    while (temp != NULL) {
        if (temp->pid == pid) {
            temp->n_request++;
            temp->waiting = 1;
            if (DEBUG) printf("Added request to %d\n", temp->pid);
            return 0;
        }
        temp = temp->next;
    }

    // PID not found
    return -1;
}


/// Get the PID that is waiting, with the least completed requests
unsigned int get_request(node_t** head) {
    node_t* temp = *head;
    node_t* selected = NULL;
    while (temp != NULL) {
        if (temp->waiting && (selected == NULL || temp->n_request < selected->n_request)) {
            selected = temp;
        }
        temp = temp->next;
    }

    if (selected != NULL) {
        if (DEBUG) printf("Getting request from %d\n", selected->pid);
        selected->n_request --;
        selected->waiting = 0;
        return selected->pid;
    } else {
        printf("ERROR");
        return 0;
    }
}
#endif
