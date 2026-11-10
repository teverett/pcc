#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rcc {

// One resolved Windows import (from a COFF import library, a GNU import archive member, or a DLL's
// own export table) - what fileformat/coff/HostedPeReader.h's readCoffImportObject()/
// windowsDllImports()/gnuCoffImports() produce, and what `PEImage::imports` below (and makePe()'s
// import-table/IAT emission) consume.
struct HostedImport {
    std::string symbol;
    std::string name;
    std::string library;
    std::vector<std::string> aliases;
    uint32_t thunkOffset = 0;
    uint32_t iatOffset = 0;
    uint32_t hintNameOffset = 0;
    uint16_t ordinal = 0;
    bool byOrdinal = false;
    bool isData = false;
};

// The fully laid-out, fully relocated hosted i386 Windows program - what linker/WindowsLinuxLinker.cpp's
// mergePeObjects() builds, and fileformat/coff/HostedPeExecutableWriter.h's makePe() turns into a
// real PE executable.
struct PEImage {
    std::string code;
    uint32_t bss = 0;
    uint32_t entry = 0;
    uint32_t importOffset = 0;
    uint32_t importSize = 0;
    uint32_t iatOffset = 0;
    uint32_t iatSize = 0;
    bool hasWindowsImports = false;
    std::vector<HostedImport> imports;
    std::vector<uint32_t> baseRelocations;
};

}  // namespace rcc
