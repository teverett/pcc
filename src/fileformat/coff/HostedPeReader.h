#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "fileformat/coff/PEImage.h"
#include "fileformat/archive/ArchiveReader.h"

namespace rcc {

// Reads real i386 COFF import libraries, GNU import archives, and DLL export tables for the
// hosted Windows linker (see linker/WindowsLinuxLinker.h) - a different concern from
// HostedCoffReader.h (a real COFF *object* file) and PeExportReader.h (one DLL's raw export
// table, used here as a building block).

// True if `bytes` is a short-form MS-generated COFF import object (IMAGE_FILE_MACHINE_UNKNOWN +
// the 0xffff import-object signature), as opposed to a real compiled `.obj`.
bool isCoffImportObject(const std::string &bytes);

// Parses one short-form COFF import object into the symbol/DLL/ordinal-or-name it describes.
HostedImport readCoffImportObject(const std::string &bytes);

// Builds one `HostedImport` per function a DLL exports, suitable for resolving an `-l<name>`
// argument that turned out to be a real DLL rather than an import library.
std::vector<HostedImport> windowsDllImports(const std::string &bytes, const std::string &path);

// Scans a GNU `dlltool`-style import archive's members for `.idata$*`-section-based imports
// (as opposed to MS short-form import objects, which `isCoffImportObject()` recognizes directly),
// keyed by member index.
std::unordered_map<size_t, HostedImport> gnuCoffImports(const std::vector<ArchiveMember> &members);

}  // namespace rcc
