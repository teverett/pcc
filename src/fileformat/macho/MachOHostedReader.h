#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "fileformat/ObjectFile.h"

namespace rcc {

// Reads real, already-assembled thin 64-bit arm64 Mach-O input (MH_OBJECT object files,
// MH_DYLIB shared libraries, and Apple's text-based ".tbd" stub libraries) for the hosted
// macOS linker (see linker/MacOSLinker.h) - unlike MachOFormat.h/.cpp's reader/writer
// pair, which only ever round-trips this project's own simplified freestanding layout.

// One real MH_OBJECT's or MH_DYLIB's parsed contents. For an object (`parseMachO(...,
// requireObject=true)`), `object` is the format-neutral translation unit ready for linking.
// For a dylib (`requireObject=false`), `installName`/`exportedSymbols` describe what it
// exports and `object` is left empty.
struct MachObjectInfo {
    uint32_t fileType = 0;
    uint32_t cpuType = 0;
    uint32_t cpuSubtype = 0;
    std::string installName;
    ObjectFile object;
    std::unordered_set<std::string> exportedSymbols;
    uint32_t exportTrieOffset = 0;
    uint32_t exportTrieSize = 0;
};

// Parses `bytes` as a thin 64-bit arm64 Mach-O file; `label` is only used in error messages.
// `requireObject` selects which file type is expected and which half of `MachObjectInfo` is
// populated - see its doc comment above.
MachObjectInfo parseMachO(const std::string &bytes, const std::string &label, bool requireObject);

// One parsed Apple text-based stub (.tbd) library description: its own exported symbols plus
// any other libraries it re-exports (only symbols from the targets `supportedTarget()` accepts
// are collected - see parseTbd()'s own filtering of `archs:`/`targets:` blocks).
struct StubLibrary {
    std::string installName;
    std::unordered_set<std::string> exports;
    std::vector<std::string> reexports;
    bool hasTargetMetadata = false;
    bool supportsArm64MacOS = false;
};

// Parses one .tbd file's YAML-ish text; `label` is only used in error messages.
StubLibrary parseTbd(const std::string &bytes, const std::string &label);

// Locates the installed macOS SDK (honoring $SDKROOT, then a few well-known Xcode/CLT
// locations), or an empty path if none is found.
std::filesystem::path findSdkRoot();

// Maps a dylib's install name (e.g. "/usr/lib/libobjc.A.dylib") to its .tbd stub's path under
// `sdkRoot`, or an empty path if none exists there.
std::filesystem::path stubPathForInstallName(const std::filesystem::path &sdkRoot,
                                             const std::string &installName);

// Reads the .tbd at `path` and recursively follows its `reexported-libraries:` into `exports`,
// using `seen` (canonicalized paths) to avoid revisiting a library already collected.
void collectTbdExports(const std::filesystem::path &path, const std::filesystem::path &sdkRoot,
                       std::unordered_set<std::string> &exports,
                       std::unordered_set<std::string> &seen);

}  // namespace rcc
