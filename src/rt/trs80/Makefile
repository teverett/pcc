# Builds trs80.c into trs80.lib, a relocatable Z80 COFF object - rcc has no
# archive/`ar` format of its own (ArchiveReader.h only *reads* archives, for
# hosted-target `-l<name>` lookup; see docs/using.md), so "library" here
# means exactly what --format=coff already produces for any object-format
# compile: one ordinary object this directory just names with a .lib
# extension by convention. `--link` takes it as a plain positional object,
# the same as any other .o - see docs/using.md's "Link mode".
RCC      ?= ../../../build/bin/rcc
TARGET   := trs80.lib

$(TARGET): trs80.c trs80.h
	$(RCC) --target=z80 --format=coff --out=$(TARGET) trs80.c

.PHONY: clean
clean:
	rm -f $(TARGET)
