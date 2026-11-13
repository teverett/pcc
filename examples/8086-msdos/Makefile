EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(EXAMPLE_DIR)build
MSDOS_DIR := $(EXAMPLE_DIR)../../src/rt/msdos
MSDOS_LIB := $(MSDOS_DIR)/msdos.lib

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.com

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=8086-dos-raw --out="$@" "$<"

# msdos.lib is src/rt/msdos/'s own build product (see that directory's
# Makefile) - built here via a sub-make rather than recompiling msdos.c
# a second time, so this example links the same object every other
# msdos.c consumer would.
$(MSDOS_LIB):
	$(MAKE) -C $(MSDOS_DIR)

$(BUILD_DIR)/main.com: $(BUILD_DIR)/main.o $(MSDOS_LIB)
	$(RCC) --link --platform=8086-dos-raw --out="$(BUILD_DIR)/main.com" "$(BUILD_DIR)/main.o" "$(MSDOS_LIB)"

# No DOS/8086 emulator is wired into this project for that - see this
# example's own README - so this only confirms main.com exists.
test: all
	test -s "$(BUILD_DIR)/main.com"

clean:
	rm -rf "$(BUILD_DIR)"
