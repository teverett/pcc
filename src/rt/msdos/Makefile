# Builds msdos.c into msdos.lib, a relocatable 8086-dos COFF object - rcc has
# no archive/`ar` format of its own (ArchiveReader.h only *reads* archives,
# for hosted-target `-l<name>` lookup; see docs/using.md), so "library" here
# means exactly what --format=coff already produces for any object-format
# compile: one ordinary object this directory just names with a .lib
# extension by convention. `--link` takes it as a plain positional object,
# the same as any other .o - see docs/using.md's "Link mode".
RCC      ?= ../../../build/bin/rcc
TARGET   := msdos.lib

$(TARGET): msdos.c msdos.h
	$(RCC) --target=8086-dos --format=coff --out=$(TARGET) msdos.c

.PHONY: clean
clean:
	rm -f $(TARGET)
