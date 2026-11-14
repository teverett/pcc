# Builds cpc.c into cpc.lib - see src/rt/trs80/Makefile's own header comment for what ".lib"
# means here (an ordinary --format=coff object, just named by convention - rcc has no archive
# format of its own).
RCC      ?= ../../../build/bin/rcc
TARGET   := cpc.lib

$(TARGET): cpc.c cpc.h
	$(RCC) --target=z80 --format=coff --out=$(TARGET) cpc.c

.PHONY: clean
clean:
	rm -f $(TARGET)
