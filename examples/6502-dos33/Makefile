EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build
APPLE2_DIR := $(EXAMPLE_DIR)../../src/rt/apple2
APPLE2_LIB := $(APPLE2_DIR)/apple2.lib

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.prodos

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=6502-prodos --dump-ir --dump-asm --out="$@" "$<"

# apple2.lib is src/rt/apple2/'s own build product (see that directory's
# Makefile) - built here via a sub-make rather than recompiling apple2.c
# a second time, so this example links the same object every other
# apple2.c consumer would.
$(APPLE2_LIB):
	$(MAKE) -C $(APPLE2_DIR)

# --base=8192 ($2000): the default $0600 sits inside the Apple ]['s own
# text page 1 ($0400-$07FF), so a long enough COUT/CROUT run would
# overwrite this program's own code out from under it as it executes.
# $2000 is also where ProDOS's own loader always enters a `SYS` file
# regardless of its directory entry's `aux_type` - see
# src/linker/factory/M6502LinkerFactory.h's own comment.
$(BUILD_DIR)/main.prodos: $(BUILD_DIR)/main.o $(APPLE2_LIB)
	$(RCC) --link --platform=6502-prodos --base=8192 --out="$@" "$(BUILD_DIR)/main.o" "$(APPLE2_LIB)"

# Same link, written out as `raw` instead of `prodos` - identical bytes
# minus the trailing file_type/aux_type footer (see
# ../../src/fileformat/prodos/ProDosFormat.h) - so CI can load and run it
# directly in py65 without having to strip that footer back off first.
# Same --base as main.prodos above - they need to stay identical bytes.
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o $(APPLE2_LIB)
	$(RCC) --link --platform=6502-raw --base=8192 --out="$@" "$(BUILD_DIR)/main.o" "$(APPLE2_LIB)"

# Runs main.raw in py65's 6502 MPU core (test.py) - see that script's own
# header comment.
test: $(BUILD_DIR)/main.raw
	python3 "test.py" "$(BUILD_DIR)/main.raw"

clean:
	rm -rf "$(BUILD_DIR)"
