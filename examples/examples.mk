EXAMPLES_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT := $(abspath $(EXAMPLES_DIR)..)

UNAME_S := $(shell uname -s)
ifeq ($(OS),Windows_NT)
RCC_EXE := rcc.exe
else ifneq (,$(findstring MINGW,$(UNAME_S)))
RCC_EXE := rcc.exe
else ifneq (,$(findstring MSYS,$(UNAME_S)))
RCC_EXE := rcc.exe
else
RCC_EXE := rcc
endif

RCC ?= $(ROOT)/build/bin/$(RCC_EXE)
