EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build
COLECO_DIR := $(EXAMPLE_DIR)../../src/rt/coleco
COLECO_LIB := $(COLECO_DIR)/coleco.lib

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.col

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=z80-coleco-colecorom --dump-ir --dump-asm --out="$@" "$<"

# coleco.lib is src/rt/coleco/'s own build product (see that directory's own
# Makefile) - built here via a sub-make rather than recompiling coleco.c a
# second time, so this example links the same object every other coleco.c
# consumer would.
$(COLECO_LIB):
	$(MAKE) -C $(COLECO_DIR)

$(BUILD_DIR)/main.col: $(BUILD_DIR)/main.o $(COLECO_LIB)
	$(RCC) --link --platform=z80-coleco-colecorom --out="$@" "$(BUILD_DIR)/main.o" "$(COLECO_LIB)"

# Same link, written out as `raw` instead of `colecorom` - identical bytes
# minus colecorom's own padding up to a standard cartridge size (see
# ../../src/fileformat/colecorom/ColecoRomFormat.h) - so CI can load and
# run it directly in a Z80 simulator without padding to worry about.
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o $(COLECO_LIB)
	$(RCC) --link --platform=z80-coleco-raw --out="$@" "$(BUILD_DIR)/main.o" "$(COLECO_LIB)"

# Runs main.raw on a real Z80 core (test.py) - see that script's own
# header comment.
test: $(BUILD_DIR)/main.raw
	python3 "test.py" "$(BUILD_DIR)/main.raw"

clean:
	rm -rf "$(BUILD_DIR)"
