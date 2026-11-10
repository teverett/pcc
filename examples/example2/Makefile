EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(EXAMPLE_DIR)build

include $(EXAMPLE_DIR)../examples.mk
TARGET := m30626fjpfp

.PHONY: all clean

all: $(BUILD_DIR)/main.bin

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(PCC) --target=$(TARGET) --format=coff --dump-ir --dump-asm --out="$@" "$<"

$(BUILD_DIR)/main.bin: $(BUILD_DIR)/main.o
	$(PCC) --link --target=$(TARGET) --format=flat --out="$@" "$<"

clean:
	rm -rf "$(BUILD_DIR)"
