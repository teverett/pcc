# Builds coleco.c into coleco.lib - see src/rt/trs80/Makefile's own header
# comment for what ".lib" means here (an ordinary --format=coff object,
# just named by convention - rcc has no archive format of its own).
RCC      ?= ../../../build/bin/rcc
TARGET   := coleco.lib

$(TARGET): coleco.c coleco.h
	$(RCC) --target=z80-coleco --format=coff --out=$(TARGET) coleco.c

.PHONY: clean
clean:
	rm -f $(TARGET)
