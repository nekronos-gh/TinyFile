#ifndef __TINY_FILE_LIB__
#define __TINY_FILE_LIB__

#include <mqueue.h>

#define TINY_FILE_QUEUE "/tinyservice"

#define HELLO 0x00
#define COMPRESS_START 0x01
typedef struct message_main {
    unsigned int type;
	unsigned int content;
} message_main_t;


#define CHUNKS  0x00
#define WRITE_OK  0x01
typedef struct message_compress {
    unsigned int type;
	unsigned int content1;
    unsigned int content2;
} message_compress_t;

int compress_file(mqd_t my_queue, mqd_t tf_queue, const char *path_in, const char *path_out);
int init_communication(mqd_t *my_queue, mqd_t *tf_queue);
int close_communication(mqd_t my_queue, mqd_t tf_queue);

#endif
