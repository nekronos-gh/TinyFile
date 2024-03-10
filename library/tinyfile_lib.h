#ifndef __TINY_FILE_LIB__
#define __TINY_FILE_LIB__

#include <mqueue.h>

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

typedef struct call_status{
    mqd_t my_queue;
    mqd_t tf_queue;
    char *path_in;
    char *path_out;
    int finished; // Pointer to shared variable indicating if the task is finished
    pthread_spinlock_t spinlock; // Pointer to spinlock for synchronizing access to finished
} call_status_t;


#define EMPTY 0x00
#define RAW 0x01
#define COMPRESSED 0x02
#define DONE_LIB 0x03
#define DONE_SER 0x04

int init_communication(call_status_t *status);
void set_path(call_status_t *status, char *path_in, char *path_out);
int compress_file(call_status_t *status);
void compress_file_async(call_status_t *status);
void compress_file_await(call_status_t *status);
int close_communication(call_status_t *status);


#endif
