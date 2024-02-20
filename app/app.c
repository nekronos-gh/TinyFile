#include <stdio.h>
#include <stdlib.h>
#include "tinyfile_lib.h"

int main() {
    const char *message = "Compress input/sloppy_file.gov";

    printf("APP: Calling library (%s)\n", message);
    send_message(message);
    return 0;
}
