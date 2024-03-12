CC=gcc
CFLAGS=-I.
LIBS=-L./library -ltinyfile
LIB_NAME=tinyfile

# Output directory for binaries
BIN_DIR=./bin

# Source directories
SERVICE_DIR=./service
LIBRARY_DIR=./library
APP_DIR=./app

# Target binaries
SERVICE=$(BIN_DIR)/tinyfile_service
LIBRARY=$(LIBRARY_DIR)/lib$(LIB_NAME).a
APPLICATION=$(BIN_DIR)/sample_app

.PHONY: all service library app clean

all: service library app

service: $(SERVICE)

library: $(LIBRARY)

app: $(APPLICATION)

# Service target

$(SERVICE): $(SERVICE_DIR)/service.c
	        $(CC) $(CFLAGS) $< -o $@ -pthread -lrt -lsnappy

# Library target (static library)
$(LIBRARY): $(LIBRARY_DIR)/tinyfile_lib.c
	$(CC) -c $(CFLAGS) $< -o $(LIBRARY_DIR)/main.o
	ar rcs $@ $(LIBRARY_DIR)/main.o

# Application target
$(APPLICATION): $(APP_DIR)/app.c $(LIBRARY)
	$(CC) $(CFLAGS) -I$(LIBRARY_DIR) $< -o $@ $(LIBS) -pthread -lrt

evade_taxes:
	 rm ./input/tests/compressed/*

clean:
	rm -f $(SERVICE) $(LIBRARY) $(APPLICATION) $(LIBRARY_DIR)/*.o
