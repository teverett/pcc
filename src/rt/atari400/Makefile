# Builds atari400.c into atari400.lib, a relocatable 6502 COFF object -
# rcc has no archive/`ar` format of its own (ArchiveReader.h only *reads*
# archives, for hosted-target `-l<name>` lookup; see docs/using.md), so
# "library" here means exactly what --format=coff already produces for
# any object-format compile: one ordinary object this directory just
# names with a .lib extension by convention. `--link` takes it as a
# plain positional object, the same as any other .o - see docs/using.md's
# "Link mode".
RCC      ?= ../../../build/bin/rcc
TARGET   := atari400.lib

$(TARGET): atari400.c atari400.h
	$(RCC) --target=6502 --format=coff --out=$(TARGET) atari400.c

.PHONY: clean
clean:
	rm -f $(TARGET)
