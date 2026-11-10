#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace pcc {

// One real ELF shared library (`.so`) this program links against - what
// fileformat/elf/HostedElfReader.h's readElfDynamicLibrary() parses, and what `ELFImage::dynamicLibraries`
// below (and makeElf()'s DT_NEEDED entries) consume. `asNeeded` is linking-time-only bookkeeping
// (linker/WindowsLinuxLinker.cpp drops an unused `--as-needed` library before building the final `ELFImage`).
struct DynamicLibrary {
    std::string name;
    struct Export {
        std::string version;
        uint8_t type = 0;
    };
    std::unordered_map<std::string, Export> exports;
    bool asNeeded = false;
};

// One ELF symbol imported from a `DynamicLibrary`, resolved to a PLT/GOT slot pair.
struct DynamicImport {
    std::string symbol;
    std::string library;
    uint32_t pltOffset = 0;
    uint32_t gotOffset = 0;
    std::string version;
};

// The fully laid-out, fully relocated hosted i386 Linux program - what linker/WindowsLinuxLinker.cpp's
// mergeElfObjects() builds, and fileformat/elf/HostedElfExecutableWriter.h's makeElf() turns into a
// real ELF executable.
struct ELFImage {
    std::string code;
    uint32_t bss = 0;
    uint32_t entry = 0;
    std::vector<DynamicImport> dynamicImports;
    std::vector<DynamicLibrary> dynamicLibraries;
    uint32_t gotOffset = 0;
};

}  // namespace pcc
