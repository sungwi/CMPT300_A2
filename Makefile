# Makefile for compiling the 'stalk' program

# Compiler settings - Can be customized.
CC=gcc
CFLAGS=-Iinclude -Wall
LDFLAGS=-Llib -lpthread

# Source and object files
SRC_DIR=src
LIB_DIR=lib
OBJ=$(SRC_DIR)/stalk.o $(SRC_DIR)/thread.o $(LIB_DIR)/list.o

# Executable name
EXEC=stalk

# Default target
all: $(EXEC) 

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean install

clean:
	rm -f $(SRC_DIR)/*.o $(EXEC)

install:
	@cp $(EXEC) /usr/local/bin/s-talk || echo "Installation failed. Try 'sudo make install' for system-wide installation."
