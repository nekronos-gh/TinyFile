#include <stdio.h>
#include <mqueue.h>

#include "tinyfile_lib.h"

int main() {

	call_status_t lib_call;
    init_communication(&lib_call);
	
    // 1{
    set_path(&lib_call, "./input/tests/raw/raw1.txt", "./input/tests/compressed/async1.gov");
	call_status_t *async1 = compress_file_async(&lib_call);

    // 2{
	call_status_t lib_call2;
    init_communication(&lib_call2);
    
    set_path(&lib_call2, "./input/tests/raw/raw2.txt", "./input/tests/compressed/async2.gov");
	call_status_t *async2 = compress_file_async(&lib_call2);
    
	// }1
    compress_file_await(async1);
	
	// }2
    compress_file_await(async2);
    close_communication(&lib_call);
    return 0;
}
