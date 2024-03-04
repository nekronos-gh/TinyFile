#ifndef __SERVICE
#define __SERVICE

#include <stdlib.h>

#define CLEAR 0x00
#define WRITTEN 0x01
#define COMPRESSED 0x02
#define DONE_LIB 0x03
#define DONE_SER 0x04

#define TINY_FILE_QUEUE "/tinyservice"
#define SHARED_MEMORY "/tf_mem"

typedef struct message_main {
    unsigned int type;
    unsigned int content;
} message_main_t ;

typedef struct message_compress {
    unsigned int type;
    unsigned int chunks;
    unsigned int size;
} message_compress_t;

    typedef struct node node_t;
typedef struct node {
    node_t* next;
    unsigned int pid;
    int n_request;
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
    new_node->next = *head; 

    *head = new_node;

    return 0;
}


int remove_from_llist(node_t** head, unsigned int pid) {
    if (head == NULL || *head == NULL) {
        return -1;
    }
    node_t *temp = *head, *prev = NULL;

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

#endif
