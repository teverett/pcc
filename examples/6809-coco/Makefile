EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(EXAMPLE_DIR)build
COCO_DIR := $(EXAMPLE_DIR)../../src/rt/coco
COCO_LIB := $(COCO_DIR)/coco.lib

include $(EXAMPLE_DIR)../examples.mk
TARGET := 6809

.PHONY: all clean

all: $(BUILD_DIR)/main.raw $(BUILD_DIR)/main.decb

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --target=$(TARGET) --format=coff --dump-ir --dump-asm --out="$@" "$<"

# coco.lib is src/rt/coco/'s own build product (see that directory's own
# Makefile) - built here via a sub-make rather than recompiling coco.c a
# second time, so this example links the same object every other coco.c
# consumer would.
$(COCO_LIB):
	$(MAKE) -C $(COCO_DIR)

$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o $(COCO_LIB)
	$(RCC) --link --target=$(TARGET) --format=raw --out="$@" "$(BUILD_DIR)/main.o" "$(COCO_LIB)"

# The real CoCo LOADM/CLOADM byte stream - see ../../src/fileformat/decb/DecbFormat.h.
$(BUILD_DIR)/main.decb: $(BUILD_DIR)/main.o $(COCO_LIB)
	$(RCC) --link --target=$(TARGET) --format=decb --out="$@" "$(BUILD_DIR)/main.o" "$(COCO_LIB)"

clean:
	rm -rf "$(BUILD_DIR)"
