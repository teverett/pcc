# Builds zx81.c into zx81.lib - see src/rt/trs80/Makefile's own header comment for what ".lib"
# means here (an ordinary --format=coff object, just named by convention - rcc has no archive
# format of its own).
RCC      ?= ../../../build/bin/rcc
TARGET   := zx81.lib

$(TARGET): zx81.c zx81.h
	$(RCC) --target=z80 --format=coff --out=$(TARGET) zx81.c

.PHONY: clean
clean:
	rm -f $(TARGET)
