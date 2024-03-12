#include <stdio.h>
#include <mqueue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinyfile_lib.h"


int count_files_in_list(const char* file_list_path) {
    FILE* file = fopen(file_list_path, "r");
    if (file == NULL) {
        fprintf(stderr, "Unable to open file: %s\n", file_list_path);
        return -1; 
    }

    int count = 0;
    char line[1024]; 

    while (fgets(line, sizeof(line), file) != NULL) {
        count++;
    }

    fclose(file);
    return count;
}

char* append_extension(const char* filename) {
    size_t new_filename_length = strlen(filename) + 3 + 1;
    char* new_filename = malloc(new_filename_length);
    if (new_filename == NULL) {
        fprintf(stderr, "Failed to allocate memory for new filename\n");
        return NULL;
    }

    // Use sprintf to concatenate the filename with the extension
    sprintf(new_filename, "%s%s", filename, ".tf");

    return new_filename;
}

void single_file(int is_async, char* file) {

	call_status_t lib_call;
    char* output = append_extension(file);
    init_communication(&lib_call, FIRST_CALL);
    set_path(&lib_call, file, output);
        
    if (is_async) {
	    call_status_t *async = compress_file_async(&lib_call);
        compress_file_await(async);
        // destroy_status(async);
    }
    else {
        compress_file(&lib_call);
    }

    close_communication(&lib_call, LAST_CLOSE);
    // destroy_status(&lib_call);
}

void multiple_files_sync(char* file_list) {
    FILE* file = fopen(file_list, "r");
    if (!file) {
        fprintf(stderr, "Failed to open file list: %s\n", file_list);
        return;
    }
    
	call_status_t lib_call;
    init_communication(&lib_call, FIRST_CALL);

    char line[1024]; 
    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = 0;
        char* output = append_extension(line);
        set_path(&lib_call, line, output);
        compress_file(&lib_call);

    }
    
    close_communication(&lib_call, LAST_CLOSE);
    // destroy_status(&lib_call);
    fclose(file);
}

void multiple_files_async(char* file_list) {
    FILE* file = fopen(file_list, "r");
    if (!file) {
        fprintf(stderr, "Failed to open file list: %s\n", file_list);
        return;
    }
    
    int n_files = count_files_in_list(file_list);
    
	call_status_t lib_call_init;
    init_communication(&lib_call_init, FIRST_CALL);

    call_status_t lib_calls[n_files];
    call_status_t* asyncs[n_files];
    int i = 0;

    char line[1024]; 
    while (fgets(line, sizeof(line), file) != NULL || i == n_files) {
        
        line[strcspn(line, "\n")] = 0;
        char* output = append_extension(line);
        set_path(&lib_calls[i], line, output);
        asyncs[i] = compress_file_async(&lib_calls[i]);
        
        i++;
    }

    for (i=0; i<n_files; i++) {
        compress_file_await(asyncs[i]);
        // destroy_status(asyncs[i]);
    }
    
    close_communication(&lib_call_init, LAST_CLOSE);
    // destroy_status(&lib_call_init);
    fclose(file);
}




int main(int argc, char *argv[]) {
    // Open server_settings.txt and read the first line
    FILE* settings_file = fopen("server_settings.txt", "r");
    if (!settings_file) {
        fprintf(stderr, "Failed to open server_settings.txt\n");
        return 1;
    }
    char server_settings[1024];
    if (fgets(server_settings, sizeof(server_settings), settings_file) == NULL) {
        fprintf(stderr, "Failed to read from server_settings.txt\n");
        fclose(settings_file);
        return 1;
    }
    server_settings[strcspn(server_settings, "\n")] = 0; // Remove newline character
    fclose(settings_file);

    // The states and sizes to iterate over
    const char* states[] = {"1", "0"};
    const char* file_sizes[] = {"Tiny", "Small", "Medium", "Large", "Huge"};
    int num_states = sizeof(states) / sizeof(states[0]);
    int num_sizes = sizeof(file_sizes) / sizeof(file_sizes[0]);

    // Iterate over states and sizes
    for (int i = 0; i < num_states; i++) {
        for (int j = 0; j < num_sizes; j++) {
            char arg[1024];
            snprintf(arg, sizeof(arg), "./input/test_%s", file_sizes[j]);

            char settings_env[2048];
            snprintf(settings_env, sizeof(settings_env), "%s,%s,%s", server_settings, states[i], file_sizes[j]);
            setenv("SETTINGS", settings_env, 1);

            if (strcmp(states[i], "1") == 0) {
                multiple_files_sync(arg);
            } else if (strcmp(states[i], "0") == 0) {
                multiple_files_async(arg);
            }
        }
    }

    return 0;
}
