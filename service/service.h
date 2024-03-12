#ifndef __SERVICE
#define __SERVICE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define DEBUG 0

#define TINY_FILE_QUEUE "/tinyservice"
#define SHARED_MEMORY "/tf_mem"

#define INIT 0x00
#define REQUEST 0x01
#define CLOSE 0x02
typedef struct message_main {
    unsigned int type;
	unsigned int pid;
    unsigned int tid;
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


typedef struct request_node request_node_t;
typedef struct request_node {
    request_node_t* next;
    unsigned int tid; 
} request_node_t;

typedef struct process_node process_node_t;
typedef struct process_node {
    unsigned int pid;
    process_node_t* next;
    request_node_t* request;
    int total_requests;
    int live_requests;
} process_node_t;


typedef struct worker_thread_args {
    process_node_t* root;
    pthread_mutex_t* mutex;
    size_t n_chunks;
    size_t chunk_size;
} worker_thread_args_t;


process_node_t* new_process_node(unsigned int pid) {
    process_node_t* node = malloc(sizeof(process_node_t));
    if (node != NULL) {
        // Pointing to itself by default
        node->next = node;
        node->pid = pid;
        node->total_requests = 0;
        node->live_requests= 0;
    }
    return node;
}

void free_process_node(process_node_t* node) {
    if (node != NULL) {
        free(node);
    }
}

void add_process(pthread_mutex_t* mutex, process_node_t* node, unsigned int pid) {

    // Get lock for curr
    pthread_mutex_lock(mutex);

    // Add new node after curr
    process_node_t* new_node = new_process_node(pid);
    new_node->next = node->next;
    node->next = new_node;
    // Release curr
    pthread_mutex_unlock(mutex);

}


void remove_process(pthread_mutex_t* mutex, process_node_t* node, unsigned int pid) {
    if (node == NULL) return;

    pthread_mutex_lock(mutex);

    // Only one node in the list
    if (node->next == node) {         
        if (node->pid == pid) {
            free_process_node(node);
        } else {
            perror("Error removing process\n");
        }
        pthread_mutex_unlock(mutex);
        return;
    }

    process_node_t* curr = node;
    process_node_t* prev = NULL;
    
    do {
        prev = curr;
        curr = curr->next;
    } while (curr != node && curr->pid != pid);

    if (curr->pid == pid) { 
        
        // adjust links
        prev->next = curr->next;
        free_process_node(curr); 

    } else {
        perror("Error removing process\n");
    }
    
    pthread_mutex_unlock(mutex);
}

void _add_request(process_node_t* node, unsigned int tid) {
    
    request_node_t* curr = node->request;
    request_node_t* new_request = malloc(sizeof(request_node_t));
    node->total_requests++;
    node->live_requests++;
    
    if (curr == NULL) { 
        new_request->next = NULL;
        new_request->tid = tid;
        node->request = new_request;
        return;
    }

    while (curr->next != NULL) {
        curr = curr->next;
    }

    new_request->next = NULL;
    new_request->tid = tid;
    curr->next = new_request;

}

void add_request(pthread_mutex_t* mutex, process_node_t* node, unsigned int pid, unsigned int tid) {
    if (node == NULL) return;

    pthread_mutex_lock(mutex);

    // Only one node in the list
    if (node->next == node) {         
        if (node->pid == pid) {
            _add_request(node, tid);
        } else {
            // XXX: ??
            printf("Error adding request");
            exit(EXIT_FAILURE); 
        }
        pthread_mutex_unlock(mutex);
        return;
    }

    process_node_t* curr = node;
    process_node_t* prev = NULL;

    do {
        prev = curr;
        curr = curr->next;
    } while (curr != node && curr->pid != pid);

    if (curr->pid == pid) { 
        // adjust links
        _add_request(curr, tid);

    } else {
        // XXX: ??
        printf("Error adding request");
        exit(EXIT_FAILURE); 
    }
}


unsigned int pop_request(process_node_t* node) {
    
    request_node_t* req = node->request;
    if (!req) {
        // XXX: ??
        printf("Error popping request");
        exit(EXIT_FAILURE);
    }
    node->request = req->next;
    node->live_requests--;
    unsigned int tid = req->tid;
    free(req);
    return tid;
}

/// Find the node with less total request and at least one live
unsigned int get_request(pthread_mutex_t* mutex, process_node_t* node) {
    
    pthread_mutex_lock(mutex);

    if (node == NULL) {
        printf("Error getting request");
        exit(EXIT_FAILURE); 
    }

    process_node_t* curr = node;
    process_node_t* candidate = NULL;
    int found_live_requests = 0;
    unsigned int request_id;
    
    do {
        if (curr->live_requests > 0) {
            found_live_requests = 1; 
            if (candidate == NULL || curr->total_requests < candidate->total_requests) {
                candidate = curr;
            }
        }
        curr = curr->next;
    } while (curr != node);

    if (!candidate) {
        request_id = 0;
    }
    else {
        request_id = pop_request(candidate);
    }

    pthread_mutex_unlock(mutex);
    return request_id;
}

#endif
