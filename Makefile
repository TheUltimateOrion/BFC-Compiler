CC        := clang
CFLAGS    := -Wall -Wextra -pedantic -g
TARGET    := bfc
OBJ_DIR   := obj

SRCS      := $(wildcard *.c)
OBJS      := $(patsubst %.c, $(OBJ_DIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(TARGET)
