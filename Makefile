CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c99 -O2
LDFLAGS = -lm

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

SRCS = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/tokenizer.c \
	$(SRC_DIR)/file_loader.c \
	$(SRC_DIR)/index.c \
	$(SRC_DIR)/search.c \
	$(SRC_DIR)/query_parser.c \
	$(SRC_DIR)/ranking.c \
	$(SRC_DIR)/trie.c \
	$(SRC_DIR)/avl_tree.c
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
TARGET = search_engine

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR):
	if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q $(BUILD_DIR)\* 2>nul || rem
	del /Q $(TARGET).exe 2>nul || rem
