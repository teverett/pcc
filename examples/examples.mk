EXAMPLES_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT := $(abspath $(EXAMPLES_DIR)..)

UNAME_S := $(shell uname -s)
ifeq ($(OS),Windows_NT)
PCC_EXE := pcc.exe
else ifneq (,$(findstring MINGW,$(UNAME_S)))
PCC_EXE := pcc.exe
else ifneq (,$(findstring MSYS,$(UNAME_S)))
PCC_EXE := pcc.exe
else
PCC_EXE := pcc
endif

PCC ?= $(ROOT)/build/bin/$(PCC_EXE)
