#include "ArchiveReader.h"

#include <limits>
#include <stdexcept>
#include <string_view>

namespace rcc {
namespace {

[[noreturn]] void fail(const std::string &message) {
    throw std::runtime_error("ArchiveReader: " + message);
}

size_t decimal(std::string_view text, const char *field) {
    while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
    if (text.empty()) fail(std::string("empty ") + field);
    size_t result = 0;
    for (char c : text) {
        if (c < '0' || c > '9') fail(std::string("invalid ") + field);
        size_t digit = static_cast<size_t>(c - '0');
        if (result > (std::numeric_limits<size_t>::max() - digit) / 10) {
            fail(std::string("overflow in ") + field);
        }
        result = result * 10 + digit;
    }
    return result;
}

}  // namespace

std::vector<ArchiveMember> readArchive(const std::string &bytes) {
    constexpr size_t headerSize = 60;
    if (bytes.compare(0, 8, "!<arch>\n") != 0) fail("invalid archive magic");
    std::vector<ArchiveMember> members;
    std::string longNames;
    size_t offset = 8;
    while (offset < bytes.size()) {
        if (bytes.size() - offset < headerSize) fail("truncated member header");
        std::string_view header(bytes.data() + offset, headerSize);
        if (header.substr(58, 2) != "`\n") fail("invalid member header");
        size_t size = decimal(header.substr(48, 10), "member size");
        size_t start = offset + headerSize;
        if (size > bytes.size() - start) fail("truncated member data");
        std::string name(header.substr(0, 16));
        while (!name.empty() && name.back() == ' ') name.pop_back();
        size_t dataOffset = start;
        size_t dataSize = size;
        if (name == "//") {
            longNames.assign(bytes, start, size);
        } else if (name == "/" || name == "/SYM64/" || name == "__.SYMDEF" ||
                   name == "__.SYMDEF SORTED" || name == "__.SYMDEF_64" ||
                   name == "__.SYMDEF_64 SORTED") {
        } else {
            if (name.compare(0, 3, "#1/") == 0) {
                size_t length = decimal(std::string_view(name).substr(3), "extended name length");
                if (length > size) fail("extended name exceeds member size");
                name.assign(bytes, start, length);
                while (!name.empty() && name.back() == '\0') name.pop_back();
                dataOffset += length;
                dataSize -= length;
            } else if (!name.empty() && name[0] == '/') {
                size_t index = decimal(std::string_view(name).substr(1), "long name offset");
                if (index >= longNames.size() ||
                    (index != 0 && longNames[index - 1] != '\n' && longNames[index - 1] != '\0')) {
                    fail("invalid long name offset");
                }
                size_t end = longNames.find_first_of("\n\0", index, 2);
                if (end == std::string::npos) fail("unterminated long name");
                name = longNames.substr(index, end - index);
                if (!name.empty() && name.back() == '/') name.pop_back();
            } else if (!name.empty() && name.back() == '/') {
                name.pop_back();
            }
            if (name.empty()) fail("empty member name");
            if (name != "__.SYMDEF" && name != "__.SYMDEF SORTED" &&
                name != "__.SYMDEF_64" && name != "__.SYMDEF_64 SORTED") {
                members.push_back({std::move(name), bytes.substr(dataOffset, dataSize)});
            }
        }
        offset = start + size;
        if (size % 2 != 0) {
            if (offset >= bytes.size() || bytes[offset] != '\n') fail("invalid member padding");
            ++offset;
        }
    }
    return members;
}

}  // namespace rcc
