# Makefile for pdp-c-compiler (pcc): a C-subset compiler with a
# pluggable codegen backend per target (see src/codegen/Codegen.h) - today
# pdp7 and 8086, each emitting that target's own assembly and (via
# that target's own real assembler) a real binary. Mirrors
# ../pdp-assembler/Makefile's approach: generates a C++ lexer/parser
# from grammar/CLexer.g4/CParser.g4 with the ANTLR4 tool, builds
# against the ANTLR4 C++ runtime, pure make - no CMake.
#
# Local dev (macOS / Homebrew):
#   brew install antlr antlr4-cpp-runtime
#   make
#
# Anywhere else (no Homebrew), point ANTLR at a downloaded tool jar
# and ANTLR4_RUNTIME_PREFIX at wherever that platform's own C++
# runtime package puts its headers/lib - see .github/workflows/ci.yml
# for the exact package + prefix this uses on Linux (apt's
# libantlr4-runtime-dev, prefix /usr) and Windows (MSYS2's
# mingw-w64-x86_64-antlr4-runtime-cpp, prefix /mingw64):
#   make ANTLR="java -jar /path/to/antlr-4.13.2-complete.jar" \
#        ANTLR4_RUNTIME_PREFIX=/usr

ANTLR                  ?= antlr
ANTLR4_RUNTIME_PREFIX  ?= $(shell brew --prefix antlr4-cpp-runtime 2>/dev/null || echo /usr/local)

GRAMMAR_DIR  := grammar
GEN_DIR      := build/generated
OBJ_DIR      := build/obj
BIN_DIR      := build/bin
SRC_DIR      := src

TARGET       := $(BIN_DIR)/pcc

CXX          ?= c++
CXXFLAGS     ?= -std=c++17 -O2 -Wall -Wextra -pthread
CPPFLAGS     := -I$(ANTLR4_RUNTIME_PREFIX)/include/antlr4-runtime \
                -I$(GEN_DIR) -I$(SRC_DIR)
LDFLAGS      := -L$(ANTLR4_RUNTIME_PREFIX)/lib
LDLIBS       := -lantlr4-runtime

GENERATED_SRCS := \
    $(GEN_DIR)/CLexer.cpp \
    $(GEN_DIR)/CParser.cpp \
    $(GEN_DIR)/CParserBaseVisitor.cpp \
    $(GEN_DIR)/CParserVisitor.cpp \
    $(GEN_DIR)/PccAsmLexer.cpp \
    $(GEN_DIR)/PccAsmParser.cpp \
    $(GEN_DIR)/PccAsmBaseVisitor.cpp \
    $(GEN_DIR)/PccAsmVisitor.cpp \
    $(GEN_DIR)/PccIRLexer.cpp \
    $(GEN_DIR)/PccIRParser.cpp \
    $(GEN_DIR)/PccIRParserBaseVisitor.cpp \
    $(GEN_DIR)/PccIRParserVisitor.cpp

SPACE :=
SPACE +=

# Each backend lives in its own subdirectory under src/codegen/ -
# see Codegen.h. Adding a new backend means adding its .cpp here and
# to BACKEND_DIRS below (for vpath), plus wiring it into main.cpp's
# makeBackend(). src/ast/, src/ir/, src/coff/ and src/preprocessor/
# hold code shared across backends (AST.h/BuildAst.*;
# IR.h/IRBuilder.*/IRPrinter.*; CoffWriter.* - the `--format=coff`
# writer both assemblers use; Preprocessor.* - the `#include`/
# `#define`/`#if` etc. pass that runs on every source file before
# ANTLR ever sees it) - not backend-specific, but still their own
# subdirectories needing the same vpath treatment.
BACKEND_DIRS := $(SRC_DIR)/codegen/pdp7 $(SRC_DIR)/codegen/x86_8086
AST_DIR      := $(SRC_DIR)/ast
IR_DIR       := $(SRC_DIR)/ir
COFF_DIR     := $(SRC_DIR)/coff
LINKER_DIR   := $(SRC_DIR)/linker
FORMAT_DIR   := $(LINKER_DIR)/format
PREPROCESSOR_DIR := $(SRC_DIR)/preprocessor
OPTIMIZERS_DIR    := $(SRC_DIR)/optimizers

HAND_SRCS := \
    $(SRC_DIR)/main.cpp \
    $(COFF_DIR)/CoffWriter.cpp \
    $(COFF_DIR)/CoffReader.cpp \
    $(LINKER_DIR)/Pdp7Linker.cpp \
    $(LINKER_DIR)/I8086Linker.cpp \
    $(FORMAT_DIR)/Pdp7TapePacking.cpp \
    $(FORMAT_DIR)/CoffLinkWriter.cpp \
    $(FORMAT_DIR)/RimWriter.cpp \
    $(FORMAT_DIR)/PtrWriter.cpp \
    $(FORMAT_DIR)/A7outWriter.cpp \
    $(FORMAT_DIR)/ListWriter.cpp \
    $(FORMAT_DIR)/BinWriter.cpp \
    $(FORMAT_DIR)/AoutWriter.cpp \
    $(PREPROCESSOR_DIR)/Preprocessor.cpp \
    $(AST_DIR)/BuildAst.cpp \
    $(IR_DIR)/IRBuilder.cpp \
    $(IR_DIR)/IRPrinter.cpp \
    $(IR_DIR)/IRValidator.cpp \
    $(OPTIMIZERS_DIR)/ConstantFolding.cpp \
    $(OPTIMIZERS_DIR)/DeadCodeElimination.cpp \
    $(SRC_DIR)/codegen/pdp7/Pdp7Codegen.cpp \
    $(SRC_DIR)/codegen/pdp7/Pdp7Assembler.cpp \
    $(SRC_DIR)/codegen/x86_8086/I8086Codegen.cpp \
    $(SRC_DIR)/codegen/x86_8086/I8086Assembler.cpp

ALL_SRCS := $(GENERATED_SRCS) $(HAND_SRCS)
OBJS     := $(patsubst %,$(OBJ_DIR)/%.o,$(notdir $(ALL_SRCS)))

vpath %.cpp $(GEN_DIR):$(SRC_DIR):$(AST_DIR):$(IR_DIR):$(COFF_DIR):$(LINKER_DIR):$(FORMAT_DIR):$(PREPROCESSOR_DIR):$(OPTIMIZERS_DIR):$(subst $(SPACE),:,$(BACKEND_DIRS))

.PHONY: all clean generate build test

# Same two-step reason as ../pdp-assembler/Makefile: the generated
# sources must exist on disk before make computes the object-file
# pattern rules, so generation runs in its own recursive `make`
# invocation ahead of the build.
all:
	$(MAKE) generate
	$(MAKE) build

build: $(TARGET)

# --- Grammar generation --------------------------------------------

# Split into CLexer.g4/CParser.g4 (rather than one combined C.g4) so
# `asm { ... }` (CLexer.g4) can use a real lexer mode to capture its
# raw body verbatim - ANTLR4 lexical modes aren't available in a
# combined parser+lexer grammar. One `antlr` invocation with both
# files (lexer first) generates the lexer's .tokens file the parser's
# `tokenVocab` option needs, same as ANTLR's own multi-file convention.
$(GEN_DIR)/CLexer.cpp $(GEN_DIR)/CParser.cpp $(GEN_DIR)/CParserBaseVisitor.cpp $(GEN_DIR)/CParserVisitor.cpp: $(GRAMMAR_DIR)/CLexer.g4 $(GRAMMAR_DIR)/CParser.g4
	@mkdir -p $(GEN_DIR)
	$(ANTLR) -Dlanguage=Cpp -visitor -no-listener -o $(abspath $(GEN_DIR)) \
	    $(abspath $(GRAMMAR_DIR)/CLexer.g4) $(abspath $(GRAMMAR_DIR)/CParser.g4)

# The one shared assembly-IR grammar every backend's own in-process
# assembler parses - see grammar/PccAsm.g4's own header comment.
$(GEN_DIR)/PccAsmLexer.cpp $(GEN_DIR)/PccAsmParser.cpp $(GEN_DIR)/PccAsmBaseVisitor.cpp $(GEN_DIR)/PccAsmVisitor.cpp: $(GRAMMAR_DIR)/PccAsm.g4
	@mkdir -p $(GEN_DIR)
	$(ANTLR) -Dlanguage=Cpp -visitor -no-listener -o $(abspath $(GEN_DIR)) $(abspath $(GRAMMAR_DIR)/PccAsm.g4)

# The grammar for IRPrinter.cpp's own output (see
# grammar/PccIRLexer.g4's header comment and src/ir/IRValidator.h) -
# split into a lexer/parser pair for the same `asm {...}`-raw-text-
# mode reason as CLexer.g4/CParser.g4 above.
$(GEN_DIR)/PccIRLexer.cpp $(GEN_DIR)/PccIRParser.cpp $(GEN_DIR)/PccIRParserBaseVisitor.cpp $(GEN_DIR)/PccIRParserVisitor.cpp: $(GRAMMAR_DIR)/PccIRLexer.g4 $(GRAMMAR_DIR)/PccIRParser.g4
	@mkdir -p $(GEN_DIR)
	$(ANTLR) -Dlanguage=Cpp -visitor -no-listener -o $(abspath $(GEN_DIR)) \
	    $(abspath $(GRAMMAR_DIR)/PccIRLexer.g4) $(abspath $(GRAMMAR_DIR)/PccIRParser.g4)

generate: $(GENERATED_SRCS)

# --- Compilation ------------------------------------------------------

$(OBJ_DIR)/%.cpp.o: %.cpp | $(GENERATED_SRCS)
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

# Header dependencies: without this, editing a backend's .h alone
# (which main.cpp also includes) wouldn't trigger a rebuild of
# main.cpp.o by mtime rules, silently linking it - built against the
# old class layout - against a Backend .cpp.o built against the new one.
-include $(OBJS:.o=.d)

# --- Housekeeping -------------------------------------------------

clean:
	rm -rf build

# Runs every backend's test suite in turn - see tests/run_tests.sh's
# --target. A backend whose own target tool isn't installed (pdp7's
# simulator, 8086's qemu) fails loudly rather than being skipped
# silently; install it or run `tests/run_tests.sh $(TARGET) --target=<one>`
# directly to check just one.
test: all
	./tests/run_tests.sh $(TARGET) --target=pdp7
	./tests/run_tests.sh $(TARGET) --target=8086
