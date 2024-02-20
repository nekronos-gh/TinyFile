# API
```c
// Set up library
int tf_init(int async){
}

// Gets the path of the file and the path for the compressed file
// Outputs code
int tf_compress_file(char* path_in, char* path_out) {
}
```



# Library
 - Check if path out is writeable
 - Get the size of the file
 - Choose appropiate message queue
 - !! Set up memory
 - Send message

