#include <stdio.h>
#include <stdlib.h>
#include "tinyfile_lib.h"

int main() {
    const char *message = "Compress /bin/input/sloppy_file.gov";

    printf("APP: Calling library\n");
    send_message(message);
    return 0;
}
