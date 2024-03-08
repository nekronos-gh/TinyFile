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


#define EMPTY 0x00
#define RAW 0x01
#define COMPRESSED 0x02
#define DONE_LIB 0x03
#define DONE_SER 0x04

int init_communication(mqd_t *my_queue, mqd_t *tf_queue);
int compress_file(mqd_t my_queue, mqd_t tf_queue, const char *path_in, const char *path_out);
int close_communication(mqd_t my_queue, mqd_t tf_queue);

#endif
