EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(EXAMPLE_DIR)build

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.bin

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=6502-raw --dump-ir --dump-asm --out="$@" "$<"

$(BUILD_DIR)/main.bin: $(BUILD_DIR)/main.o
	$(RCC) --link --platform=6502-raw --out="$@" "$<"

# Runs main.bin in py65's 6502 MPU core (test.py) - see that script's own
# header comment.
test: $(BUILD_DIR)/main.bin
	python3 "$(EXAMPLE_DIR)test.py" "$(BUILD_DIR)/main.bin"

clean:
	rm -rf "$(BUILD_DIR)"
