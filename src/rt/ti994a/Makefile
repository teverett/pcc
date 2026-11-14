# Builds ti994a.c into ti994a.lib - see src/rt/trs80/Makefile's own header comment for what
# ".lib" means here (an ordinary --format=coff object, just named by convention - rcc has no
# archive format of its own).
RCC      ?= ../../../build/bin/rcc
TARGET   := ti994a.lib

$(TARGET): ti994a.c ti994a.h
	$(RCC) --target=tms9900 --format=coff --out=$(TARGET) ti994a.c

.PHONY: clean
clean:
	rm -f $(TARGET)
