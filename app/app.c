#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>

#include "tinyfile_lib.h"

int main() {
    const char *message = "Compress input/sloppy_file.gov";

	mqd_t my_queue, tf_queue;
	init_communication(&my_queue, &tf_queue);

	compress_file(my_queue, tf_queue, "sloppy.txt", "sloppy.gov");

	close_communication(my_queue, tf_queue);
    return 0;
}
