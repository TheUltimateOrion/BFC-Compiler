CC            ?= cc
CSTD          := c23
CONFIG        ?= debug

SRC_DIR       := src
INCLUDE_DIR   := include
BUILD_ROOT    := build

DOXYGEN       ?= doxygen
DOXYFILE      ?= Doxyfile
DOCS_DIR      ?= docs/doxygen
CLANG_FORMAT  ?= clang-format
CLANG_TIDY    ?= clang-tidy

CONFIG_DIR    := $(BUILD_ROOT)/$(CONFIG)
OBJ_DIR       := $(CONFIG_DIR)/obj
TARGET        := $(CONFIG_DIR)/bfc

SRCS          := $(sort $(shell find $(SRC_DIR) -name '*.c'))
HEADERS       := $(sort $(shell find $(INCLUDE_DIR) -name '*.h'))
OBJS          := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS          := $(OBJS:.o=.d)
FORMAT_FILES  := $(SRCS) $(HEADERS)

VERSION_FILE  := VERSION
VERSION       := $(strip $(shell cat $(VERSION_FILE)))

# Compiler configuration

CPPFLAGS      += -I$(INCLUDE_DIR)
CPPFLAGS      += -DBFC_VERSION=\"$(VERSION)\"

DEPFLAGS      := -MMD -MP

COMMON_CFLAGS := \
	-std=$(CSTD) \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wuninitialized \
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

	LDFLAGS := -fsanitize=address,undefined
else
	$(error Unknown CONFIG '$(CONFIG)'; expected debug or release)
endif

LDLIBS += -lm

# Targets

.PHONY: all debug release clean docs clean-docs format format-check tidy

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

format:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { \
		echo "error: clang-format is not installed or not in PATH"; \
		exit 1; \
	}
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

format-check:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { \
		echo "error: clang-format is not installed or not in PATH"; \
		exit 1; \
	}
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

tidy:
	@command -v $(CLANG_TIDY) >/dev/null 2>&1 || { \
		echo "error: clang-tidy is not installed or not in PATH"; \
		exit 1; \
	}
	$(CLANG_TIDY) $(SRCS) -- $(CPPFLAGS) -std=$(CSTD)

clean:
	rm -rf $(BUILD_ROOT)

clean-docs:
	rm -rf $(DOCS_DIR)

# Build rules

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJS): $(VERSION_FILE)

-include $(DEPS)
