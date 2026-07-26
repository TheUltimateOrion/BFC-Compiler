CC        := clang
TARGET    := bfc
SRC_DIR   := src
OBJ_DIR   := obj
CSTD	  := c23

# 1. Set the default configuration to 'debug' if not specified
CONFIG    ?= debug

# 2. Define flags and paths based on the chosen configuration
BASE_CFLAGS := -Wall -Wextra -pedantic -Wuninitialized -Iinclude -std=$(CSTD)

ifeq ($(CONFIG),release)
    CFLAGS    := $(BASE_CFLAGS) -O3 -DNDEBUG
    BUILD_DIR := $(OBJ_DIR)/release
else
    CFLAGS    := $(BASE_CFLAGS) -Wconditional-uninitialized -g -O0 -fsanitize=address -fno-omit-frame-pointer
    BUILD_DIR := $(OBJ_DIR)/debug
endif

# 3. Source and Object file resolution (Always evaluates correctly)
SRCS      := $(wildcard $(SRC_DIR)/*.c)
OBJS      := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# 4. Standard Targets
.PHONY: all clean debug release

all: $(TARGET)

# Shortcut targets that call make again with the explicit config
debug:
	@$(MAKE) CONFIG=debug all

release:
	@$(MAKE) CONFIG=release all

# 5. Core Build Rules
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
