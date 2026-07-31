CC           := cc
CSTD         := c23
CONFIG       ?= debug

SRC_DIR      := src
INCLUDE_DIR  := include
BUILD_ROOT   := build

DOXYGEN      ?= doxygen
DOXYFILE     ?= Doxyfile
DOCS_DIR     ?= docs/doxygen

CONFIG_DIR   := $(BUILD_ROOT)/$(CONFIG)
OBJ_DIR      := $(CONFIG_DIR)/obj
TARGET       := $(CONFIG_DIR)/bfc

SRCS         := $(shell find $(SRC_DIR) -name '*.c')
OBJS         := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

VERSION_FILE := VERSION
VERSION      := $(strip $(shell cat $(VERSION_FILE)))

# Compiler configuration

CPPFLAGS     += -I$(INCLUDE_DIR)
CPPFLAGS     += -DBFC_VERSION=\"$(VERSION)\"

COMMON_CFLAGS := \
	-std=$(CSTD) \
	-I$(INCLUDE_DIR) \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wuninitialized \
	-Wall \
	-Wconversion \
	-Wsign-conversion \
	-Wshadow \
	-Wstrict-prototypes \
	-Wvla \
	-Werror

ifeq ($(CONFIG),release)
	CFLAGS  := $(COMMON_CFLAGS) -O3 -DNDEBUG
	LDFLAGS :=
else ifeq ($(CONFIG),debug)
	CFLAGS := $(COMMON_CFLAGS) \
		-Wconditional-uninitialized \
		-g3 \
		-O0 \
		-fsanitize=address,undefined \
		-fno-omit-frame-pointer

	LDFLAGS := -fsanitize=address
else
	$(error Unknown CONFIG '$(CONFIG)'; expected debug or release)
endif


# Targets

.PHONY: all debug release clean docs clean-docs

all: $(TARGET)

debug:
	$(MAKE) CONFIG=debug all

release:
	$(MAKE) CONFIG=release all

docs:
	@command -v $(DOXYGEN) >/dev/null 2>&1 || { \
		echo "error: Doxygen is not installed or not in PATH"; \
		exit 1; \
	}
	$(DOXYGEN) $(DOXYFILE)
	@echo "Documentation generated at $(DOCS_DIR)/html/index.html"

clean:
	rm -rf $(BUILD_ROOT)

clean-docs:
	rm -rf $(DOCS_DIR)


# Build rules

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJS): $(VERSION_FILE)