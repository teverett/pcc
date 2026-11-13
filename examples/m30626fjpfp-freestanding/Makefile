EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(EXAMPLE_DIR)build

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all clean

all: $(BUILD_DIR)/main.bin

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=m30626fjpfp-raw --dump-ir --dump-asm --out="$@" "$<"

$(BUILD_DIR)/main.bin: $(BUILD_DIR)/main.o
	$(RCC) --link --platform=m30626fjpfp-raw --out="$@" "$<"

clean:
	rm -rf "$(BUILD_DIR)"
