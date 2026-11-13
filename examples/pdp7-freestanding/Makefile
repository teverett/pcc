EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.rim

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=pdp7-rim --dump-ir --dump-asm --out="$@" "$<"

$(BUILD_DIR)/main.rim: $(BUILD_DIR)/main.o
	$(RCC) --link --platform=pdp7-rim --out="$@" "$<"

# Runs main.rim under a real PDP-7 simulator (test.py) - see that script's
# own header comment. Needs open-simh's `pdp7` on PATH.
test: $(BUILD_DIR)/main.rim
	python3 "test.py" "$(BUILD_DIR)/main.rim"

clean:
	rm -rf "$(BUILD_DIR)"
