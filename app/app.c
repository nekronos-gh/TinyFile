#include <stdio.h>
#include <mqueue.h>

#include "tinyfile_lib.h"

int main() {

	call_status_t lib_call;

	init_communication(&lib_call);
	set_path(&lib_call, "./input/tests/raw/toppy.txt", "./input/tests/compressed/toppy.gov");

	call_status_t *async1 = compress_file_async(&lib_call);

	set_path(&lib_call, "./input/tests/raw/sloppy.txt", "./input/tests/compressed/gupta.gov");

	call_status_t *async2 = compress_file_async(&lib_call);

	compress_file_await(async2);

	set_path(&lib_call, "./input/tests/raw/megatoppy.txt", "./input/tests/compressed/megatoppy.gov");
	compress_file(&lib_call);
	compress_file_await(async1);

	close_communication(&lib_call);
    return 0;
}
