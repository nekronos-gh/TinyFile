#include <stdio.h>
#include <mqueue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "tinyfile_lib.h"

typedef struct {
    int is_async; 
    char* file; 
    char* files; 
} config ;


void parse_args(int argc, char* argv[], config* options) {
    struct option long_options[] = {
        {"state", required_argument, 0, 's'},
        {"file", required_argument, 0, 'f'},
        {"files", required_argument, 0, 'l'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "s:f:l:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 's':
                if (strcmp(optarg, "SYNC") == 0) {
                    options->is_async = 0;
                } else if (strcmp(optarg, "ASYNC") == 0) {
                    options->is_async = 1;
                } else {
                    fprintf(stderr, "Invalid value for --state: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'f':
                if (options->files != NULL) {
                    fprintf(stderr, "Cannot use both --file and --files.\n");
                    exit(EXIT_FAILURE);
                }
                options->file = strdup(optarg);
                break;
            case 'l':
                if (options->file != NULL) {
                    fprintf(stderr, "Cannot use both --file and --files.\n");
                    exit(EXIT_FAILURE);
                }
                options->files = strdup(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s --state SYNC|ASYNC [--file FILEPATH | --files FILELIST]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (options->file == NULL && options->files == NULL) {
        fprintf(stderr, "Either --file or --files must be specified.\n");
        fprintf(stderr, "Usage: %s --state SYNC|ASYNC [--file FILEPATH | --files FILELIST]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

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

void single_file(int async, char* file) {

	call_status_t lib_call;
    char* output = append_extension(file);
    init_communication(&lib_call, FIRST_CALL);
    set_path(&lib_call, file, output);
        
    if (async) {
	    call_status_t *async = compress_file_async(&lib_call);
        compress_file_await(async);
    }
    else {
        compress_file(&lib_call);
    }

    close_communication(&lib_call);
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
    
    close_communication(&lib_call);
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
    while (fgets(line, sizeof(line), file) != NULL) {
        
        line[strcspn(line, "\n")] = 0;
        char* output = append_extension(line);
        set_path(&lib_calls[i], line, output);
        asyncs[i] = compress_file_async(&lib_calls[i]);
        
        i++;
    }

    for (i=0; i<n_files; i++) {
        compress_file_await(asyncs[i]);
    }
    
    close_communication(&lib_call_init);
    fclose(file);
}

int main(int argc, char *argv[]) {
    config cfg = {0};

    parse_args(argc, argv, &cfg);

    if (cfg.file != NULL)
        single_file(cfg.is_async, cfg.file);
    if (cfg.files != NULL) {
        if (cfg.is_async) {
            multiple_files_async(cfg.files);
        }
        else {
            multiple_files_sync(cfg.files);
        }
    }

    if (cfg.file) {
        free(cfg.file);
    }
    if (cfg.files) {
        free(cfg.files);
    }

    return 0;
}
