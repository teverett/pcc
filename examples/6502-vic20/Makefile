EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build
VIC20_DIR := $(EXAMPLE_DIR)../../src/rt/vic20
VIC20_LIB := $(VIC20_DIR)/vic20.lib

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.prg

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=6502-prg --dump-ir --dump-asm --out="$@" "$<"

# vic20.lib is src/rt/vic20/'s own build product (see that directory's
# Makefile) - built here via a sub-make rather than recompiling vic20.c
# a second time, so this example links the same object every other
# vic20.c consumer would.
$(VIC20_LIB):
	$(MAKE) -C $(VIC20_DIR)

# --base=$1001: the unexpanded VIC-20's own BASIC-program start address
# - where the KERNAL's own LOAD routine places (and, for a `prg` file
# whose own 2-byte header matches, autoruns) a loaded program. The
# default $0600 base instead sits inside the VIC-20's own zero-page/
# low-memory KERNAL work area, not this machine's program area at all.
$(BUILD_DIR)/main.prg: $(BUILD_DIR)/main.o $(VIC20_LIB)
	$(RCC) --link --platform=6502-prg --base='$$1001' --out="$@" "$(BUILD_DIR)/main.o" "$(VIC20_LIB)"

# Same link, written out as `raw` instead of `prg` - identical bytes minus
# the 2-byte load-address header (see ../../src/fileformat/prg/
# PrgFormat.h) - so CI can load and run it directly in py65 without
# having to strip that header back off first. Same --base as main.prg
# above - they need to stay identical bytes.
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o $(VIC20_LIB)
	$(RCC) --link --platform=6502-raw --base='$$1001' --out="$@" "$(BUILD_DIR)/main.o" "$(VIC20_LIB)"

# Runs main.raw in py65's 6502 MPU core (test.py) - see that script's own
# header comment.
test: $(BUILD_DIR)/main.raw
	python3 "test.py" "$(BUILD_DIR)/main.raw"

clean:
	rm -rf "$(BUILD_DIR)"
