EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(EXAMPLE_DIR)build
TRS80_DIR := $(EXAMPLE_DIR)../../src/rt/trs80
TRS80_LIB := $(TRS80_DIR)/trs80.lib

include $(EXAMPLE_DIR)../examples.mk
TARGET := z80

.PHONY: all clean

all: $(BUILD_DIR)/main.cmd

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --target=$(TARGET) --format=coff --dump-ir --dump-asm --out="$@" "$<"

# trs80.lib is src/rt/trs80/'s own build product (see that directory's own
# Makefile) - built here via a sub-make rather than recompiling trs80.c a
# second time, so this example links the same object every other trs80.c
# consumer would.
$(TRS80_LIB):
	$(MAKE) -C $(TRS80_DIR)

$(BUILD_DIR)/main.cmd: $(BUILD_DIR)/main.o $(TRS80_LIB)
	$(RCC) --link --target=$(TARGET) --format=cmd --out="$@" "$(BUILD_DIR)/main.o" "$(TRS80_LIB)"

# Same link, written out as `raw` instead of `cmd` - identical .text/entry
# bytes minus cmd's own TYPE/LENGTH record wrapping (see
# ../../src/fileformat/cmd/CmdFormat.h) - so CI can load and run it
# directly in a Z80 simulator without having to unwrap those records
# first.
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o $(TRS80_LIB)
	$(RCC) --link --target=$(TARGET) --format=raw --out="$@" "$(BUILD_DIR)/main.o" "$(TRS80_LIB)"

clean:
	rm -rf "$(BUILD_DIR)"
