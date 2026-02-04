CC        := clang
CFLAGS    := -Wall -Wextra -pedantic -Iinclude -g
TARGET    := bfc
SRC_DIR   := src
OBJ_DIR   := obj

SRCS      := $(wildcard $(SRC_DIR)/*.c)
OBJS      := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(TARGET)
