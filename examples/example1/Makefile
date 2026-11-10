EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(EXAMPLE_DIR)build

include $(EXAMPLE_DIR)../examples.mk

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
DEFAULT_TARGET := macos
else ifeq ($(UNAME_S),Linux)
DEFAULT_TARGET := linux
else ifneq (,$(findstring MINGW,$(UNAME_S)))
DEFAULT_TARGET := windows
else ifneq (,$(findstring MSYS,$(UNAME_S)))
DEFAULT_TARGET := windows
else
DEFAULT_TARGET := macos
endif

.PHONY: all macos linux windows clean

all: $(DEFAULT_TARGET)

macos: $(BUILD_DIR)/hello-macos
linux: $(BUILD_DIR)/hello-linux
windows: $(BUILD_DIR)/hello-windows.exe

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/hello-macos.o: hello.c | $(BUILD_DIR)
	$(RCC) --target=armv8a-macos --out="$@" "$<"

$(BUILD_DIR)/hello-macos: $(BUILD_DIR)/hello-macos.o
	$(RCC) --link --target=armv8a-macos --out="$@" "$<" -lSystem

$(BUILD_DIR)/hello-linux.o: hello.c | $(BUILD_DIR)
	$(RCC) --target=i386-linux --out="$@" "$<"

$(BUILD_DIR)/hello-linux: $(BUILD_DIR)/hello-linux.o
	$(RCC) --link --target=i386-linux --out="$@" "$<" -lc

$(BUILD_DIR)/hello-windows.obj: hello.c | $(BUILD_DIR)
	$(RCC) --target=i386-windows --out="$@" "$<"

$(BUILD_DIR)/hello-windows.exe: $(BUILD_DIR)/hello-windows.obj
	$(RCC) --link --target=i386-windows --out="$@" "$<" -lmsvcrt

clean:
	rm -rf "$(BUILD_DIR)"
