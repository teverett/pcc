#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pcc {

// Writes the final linked arm64 macOS executable (a real, loadable, code-signed Mach-O
// MH_EXECUTE) for the hosted macOS linker (see linker/MacOSLinker.h) - the "target file
// format" half of that linker, as MachOHostedReader.h is its "object file format" half.

// Where this image base-loads; every address this writer computes is `kImageBase` plus some
// file offset.
constexpr uint64_t kImageBase = 0x100000000ULL;
constexpr uint32_t kPageSize = 16384;
constexpr uint32_t kSignaturePageSize = 4096;

// This writer's own relocation-type codes for a GOT-relative load - see
// MachOHostedReader.cpp's identical, deliberately-duplicated constants.
constexpr uint16_t kGotPage21 = 7;
constexpr uint16_t kGotPageoff12 = 8;

// Tags a `MachOImage::symbols`/merge-time symbol value as living in `__DATA` (`kDataSymbolTag`) and,
// within that, in `__bss` rather than initialized `__data` (`kBssSymbolTag`) - set by
// linker/MacOSLinker.cpp's mergeObjects() and read back by both it (while patching
// relocations) and makeMachExecutable() (while emitting final symbol addresses).
constexpr uint64_t kDataSymbolTag = 1ULL << 63;
constexpr uint64_t kBssSymbolTag = 1ULL << 62;

// One unresolved symbol imported from a system dylib, resolved to a lazy-stub/GOT slot pair.
struct Import {
    std::string symbol;
    uint32_t libraryIndex = 0;
    uint32_t stubOffset = 0;
    uint32_t pointerOffset = 0;
};

// One real dylib or .tbd stub this program links against - what linker/MacOSLinker.cpp's
// loadLibrary()/loadInputs() resolve `-l`/direct dylib arguments into, and what `MachOImage::libraries`
// below (and makeMachExecutable()'s LC_LOAD_DYLIB commands) consume.
struct Library {
    std::string installName;
    std::filesystem::path filePath;
    std::filesystem::path searchDirectory;
    std::unordered_set<std::string> exports;
};

// The fully laid-out, fully relocated program this writer serializes into a real Mach-O file.
// linker/MacOSLinker.cpp's mergeObjects() builds one (code/got/data/bssSize sized and
// patched, every symbol/import resolved to its final offset); makeMachExecutable() below turns
// it into real load commands, segments, bind opcodes, symbol table and a code signature.
struct MachOImage {
    std::string code;
    std::string got;
    std::string data;
    uint32_t bssSize = 0;
    uint32_t entryOffset = 0;
    uint32_t mainOffset = 0;
    std::vector<Import> imports;
    std::vector<Library> libraries;
    std::vector<std::filesystem::path> rpaths;
    std::vector<std::pair<std::string, uint64_t>> symbols;
};

// Reads a little-endian arm64 instruction word / writes it back - the unit patchPage21() and
// patchPageoff12() operate on.
uint32_t readInstruction(const std::string &bytes, size_t offset);
void writeInstruction(std::string &bytes, size_t offset, uint32_t instruction);

// Patches an ADRP's 21-bit page-relative immediate (patchPage21) or an immediate/offset
// instruction's 12-bit page-offset field (patchPageoff12) in `code` at `site` to address
// `targetAddress`, given this image always loads `code` at `kImageBase + kPageSize`.
void patchPage21(std::string &code, uint32_t site, uint64_t targetAddress);
void patchPageoff12(std::string &code, uint32_t site, uint64_t targetAddress);

// Serializes `image` into a complete, code-signed Mach-O MH_EXECUTE. Patches each import stub's
// own GOT-pointer-load instructions in place (their target wasn't known until `__DATA`'s final
// file offset was chosen here), so `image.code` is mutated as a side effect.
std::string makeMachExecutable(MachOImage &image);

}  // namespace pcc
