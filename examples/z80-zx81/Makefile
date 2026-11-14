EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build
ZX81_DIR := $(EXAMPLE_DIR)../../src/rt/zx81
ZX81_LIB := $(ZX81_DIR)/zx81.lib

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.raw

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --target=z80 --format=coff --dump-ir --dump-asm --out="$@" "$<"

# zx81.lib is src/rt/zx81/'s own build product (see that directory's own
# Makefile) - built here via a sub-make rather than recompiling zx81.c a
# second time, so this example links the same object every other zx81.c
# consumer would.
$(ZX81_LIB):
	$(MAKE) -C $(ZX81_DIR)

# --format=raw only - see ../../src/rt/zx81/zx81.h's own header comment for why no ZX81-native
# container format is offered here.
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o $(ZX81_LIB)
	$(RCC) --link --target=z80 --format=raw --out="$@" "$(BUILD_DIR)/main.o" "$(ZX81_LIB)"

# Runs main.raw on a real Z80 core (test.py) - see that script's own
# header comment.
test: $(BUILD_DIR)/main.raw
	python3 "test.py" "$(BUILD_DIR)/main.raw"

clean:
	rm -rf "$(BUILD_DIR)"
