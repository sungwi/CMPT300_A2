# Compiler settings 
CC=gcc
CFLAGS=-Iinclude -Wall
LDFLAGS=-Llib -lpthread

# Source and object files
SRC_DIR=src
LIB_DIR=lib
OBJ=$(SRC_DIR)/s-talk.o $(SRC_DIR)/thread.o $(LIB_DIR)/list.o

# Executable name
EXEC=s-talk
LINK_NAME=s-talk
BIN_DIR=$(HOME)/bin

all: $(EXEC) link

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@

# Create a symbolic link named 's-talk' to the 'stalk' executable in ~/bin
link:
	@mkdir -p $(BIN_DIR)
	@ln -sf $(PWD)/$(EXEC) $(BIN_DIR)/$(LINK_NAME)
	@echo "Link created in $(BIN_DIR)/$(LINK_NAME)"

.PHONY: clean

clean:
	rm -f $(SRC_DIR)/*.o $(EXEC)
	rm -f $(BIN_DIR)/$(LINK_NAME)
