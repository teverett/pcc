EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build
TI994A_DIR := $(EXAMPLE_DIR)../../src/rt/ti994a
TI994A_LIB := $(TI994A_DIR)/ti994a.lib

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.raw

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --target=tms9900 --format=coff --dump-ir --dump-asm --out="$@" "$<"

# ti994a.lib is src/rt/ti994a/'s own build product (see that directory's own Makefile) - built
# here via a sub-make rather than recompiling ti994a.c a second time, so this example links the
# same object every other ti994a.c consumer would.
$(TI994A_LIB):
	$(MAKE) -C $(TI994A_DIR)

# --format=raw only - no native TI-99/4A cartridge/disk container format is implemented for
# this target (out of scope for this pass - see docs/platforms.md's own `tms9900` entry).
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o $(TI994A_LIB)
	$(RCC) --link --target=tms9900 --format=raw --out="$@" "$(BUILD_DIR)/main.o" "$(TI994A_LIB)"

# No TMS9900 emulator is available to this project (see docs/known-bugs.md) - `test` only
# confirms the build output exists, same convention m30626fjpfp-freestanding/8086-msdos/
# 6809-coco use for a target with no simulator wired in.
test: $(BUILD_DIR)/main.raw
	test -s "$(BUILD_DIR)/main.raw"

clean:
	rm -rf "$(BUILD_DIR)"
