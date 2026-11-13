EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(EXAMPLE_DIR)build
APPLE2_DIR := $(EXAMPLE_DIR)../../src/rt/apple2
APPLE2_LIB := $(APPLE2_DIR)/apple2.lib

include $(EXAMPLE_DIR)../examples.mk
TARGET := 6502

.PHONY: all clean

all: $(BUILD_DIR)/main.prodos

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --target=$(TARGET) --format=coff --dump-ir --dump-asm --out="$@" "$<"

# apple2.lib is src/rt/apple2/'s own build product (see that directory's
# Makefile) - built here via a sub-make rather than recompiling apple2.c
# a second time, so this example links the same object every other
# apple2.c consumer would.
$(APPLE2_LIB):
	$(MAKE) -C $(APPLE2_DIR)

$(BUILD_DIR)/main.prodos: $(BUILD_DIR)/main.o $(APPLE2_LIB)
	$(RCC) --link --target=$(TARGET) --format=prodos --out="$@" "$(BUILD_DIR)/main.o" "$(APPLE2_LIB)"

# Same link, written out as `flat` instead of `prodos` - identical bytes
# minus the trailing file_type/aux_type footer (see
# ../../src/fileformat/prodos/ProDosFormat.h) - so CI can load and run it
# directly in py65 without having to strip that footer back off first.
$(BUILD_DIR)/main.flat: $(BUILD_DIR)/main.o $(APPLE2_LIB)
	$(RCC) --link --target=$(TARGET) --format=flat --out="$@" "$(BUILD_DIR)/main.o" "$(APPLE2_LIB)"

clean:
	rm -rf "$(BUILD_DIR)"
