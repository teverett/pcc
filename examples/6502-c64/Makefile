EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build
C64_DIR := $(EXAMPLE_DIR)../../src/rt/c64
C64_LIB := $(C64_DIR)/c64.lib

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.prg

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=6502-prg --dump-ir --dump-asm --out="$@" "$<"

# c64.lib is src/rt/c64/'s own build product (see that directory's
# Makefile) - built here via a sub-make rather than recompiling c64.c a
# second time, so this example links the same object every other c64.c
# consumer would.
$(C64_LIB):
	$(MAKE) -C $(C64_DIR)

# --base=$0801: the C64's own BASIC-program start address - where the
# KERNAL's own LOAD routine places (and, for a `prg` file whose own
# header matches, autoruns) a loaded program. The default $0600 base
# instead sits inside the C64's own zero-page/low-memory KERNAL work
# area, not this machine's program area at all.
$(BUILD_DIR)/main.prg: $(BUILD_DIR)/main.o $(C64_LIB)
	$(RCC) --link --platform=6502-prg --base='$$0801' --out="$@" "$(BUILD_DIR)/main.o" "$(C64_LIB)"

# Same link, written out as `raw` instead of `prg` - identical bytes minus
# the 2-byte load-address header (see ../../src/fileformat/prg/
# PrgFormat.h) - so CI can load and run it directly in py65 without
# having to strip that header back off first. Same --base as main.prg
# above - they need to stay identical bytes.
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o $(C64_LIB)
	$(RCC) --link --platform=6502-raw --base='$$0801' --out="$@" "$(BUILD_DIR)/main.o" "$(C64_LIB)"

# Runs main.raw in py65's 6502 MPU core (test.py) - see that script's own
# header comment.
test: $(BUILD_DIR)/main.raw
	python3 "test.py" "$(BUILD_DIR)/main.raw"

clean:
	rm -rf "$(BUILD_DIR)"
