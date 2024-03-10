#include <stdio.h>
#include <mqueue.h>

#include "tinyfile_lib.h"

int main() {

	call_status_t lib_call;

	init_communication(&lib_call);
	set_path(&lib_call, "sloppy.txt", "sloppy.gov");
	compress_file_async(&lib_call);
	compress_file_await(&lib_call);
	close_communication(&lib_call);
    return 0;
}
