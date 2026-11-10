EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT := $(abspath $(EXAMPLE_DIR)/../..)
PCC ?= $(ROOT)/build/bin/pcc
BUILD_DIR ?= $(EXAMPLE_DIR)build
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
