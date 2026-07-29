CC          := cc
SRC_DIR     := src
INCLUDE_DIR := include
BUILD_ROOT  := build
CSTD        := c23

CONFIG ?= debug

COMMON_CFLAGS := \
	-std=$(CSTD) \
	-I$(INCLUDE_DIR) \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wuninitialized

ifeq ($(CONFIG),release)
	CFLAGS  := $(COMMON_CFLAGS) -O3 -DNDEBUG
	LDFLAGS :=
else ifeq ($(CONFIG),debug)
	CFLAGS  := $(COMMON_CFLAGS) \
		-Wconditional-uninitialized \
		-g \
		-O0 \
		-fsanitize=address \
		-fno-omit-frame-pointer

	LDFLAGS := \
		-fsanitize=address
else
	$(error Unknown CONFIG '$(CONFIG)'; expected debug or release)
endif

CONFIG_DIR := $(BUILD_ROOT)/$(CONFIG)
OBJ_DIR    := $(CONFIG_DIR)/obj
TARGET     := $(CONFIG_DIR)/bfc

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean debug release

all: $(TARGET)

debug:
	$(MAKE) CONFIG=debug all

release:
	$(MAKE) CONFIG=release all

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_ROOT)