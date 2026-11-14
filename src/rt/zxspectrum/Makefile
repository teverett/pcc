# Builds zxspectrum.c into zxspectrum.lib - see src/rt/trs80/Makefile's own header comment for
# what ".lib" means here (an ordinary --format=coff object, just named by convention - rcc has
# no archive format of its own).
RCC      ?= ../../../build/bin/rcc
TARGET   := zxspectrum.lib

$(TARGET): zxspectrum.c zxspectrum.h
	$(RCC) --target=z80 --format=coff --out=$(TARGET) zxspectrum.c

.PHONY: clean
clean:
	rm -f $(TARGET)
