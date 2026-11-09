#include "fileformat/archive/ArchiveReader.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

std::string member(const std::string &name, const std::string &data) {
    require(name.size() <= 16, "test name too long");
    std::string size = std::to_string(data.size());
    std::string header = name + std::string(16 - name.size(), ' ') +
                         std::string(12 + 6 + 6 + 8, ' ') +
                         size + std::string(10 - size.size(), ' ') + "`\n";
    return header + data + (data.size() % 2 ? "\n" : "");
}

void testShortNamesAndPadding() {
    auto members = pcc::readArchive("!<arch>\n" + member("/", "index") +
                                    member("first.o/", "abc") + member("second.o/", "defg"));
    require(members.size() == 2, "wrong member count");
    require(members[0].name == "first.o" && members[0].bytes == "abc", "incorrect first member");
    require(members[1].name == "second.o" && members[1].bytes == "defg", "incorrect second member");
}

void testExtendedNames() {
    auto archive = "!<arch>\n" + member("//", "long-object-name.o/\n") +
                   member("/0", "gnu") + member("#1/14", "bsd-longname.oBSD");
    auto members = pcc::readArchive(archive);
    require(members.size() == 2, "wrong extended member count");
    require(members[0].name == "long-object-name.o" && members[0].bytes == "gnu",
            "incorrect GNU long name");
    require(members[1].name == "bsd-longname.o" && members[1].bytes == "BSD",
            "incorrect BSD long name");
}

void rejects(const std::string &archive) {
    try {
        pcc::readArchive(archive);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error("invalid archive accepted");
}

void testMalformedArchives() {
    rejects("!<thin>\n");
    rejects("!<arch>\nshort");
    rejects("!<arch>\n" + member("bad/", "abc").substr(0, 62));
    rejects("!<arch>\n" + member("/0", "abc"));
    rejects("!<arch>\n" + member("#1/20", "abc"));
    rejects("!<arch>\n" + member("bad/", "abc").substr(0, 63));
}

void testMicrosoftLongNames() {
    std::string first = "first-windows-long-name.obj";
    std::string second = "second-windows-long-name.obj";
    std::string table = first + '\0' + second + '\0';
    auto members = pcc::readArchive("!<arch>\n" + member("//", table) +
                                   member("/0", "first") +
                                   member("/" + std::to_string(first.size() + 1), "second"));
    require(members.size() == 2, "wrong Microsoft member count");
    require(members[0].name == first && members[0].bytes == "first", "incorrect first Microsoft name");
    require(members[1].name == second && members[1].bytes == "second", "incorrect second Microsoft name");
}

void testMalformedMicrosoftLongNames() {
    rejects("!<arch>\n" + member("//", std::string("long-name.obj\0", 14)) + member("/2", "data"));
    rejects("!<arch>\n" + member("//", "unterminated.obj") + member("/0", "data"));
}

}  // namespace

int main(int argc, char **argv) {
    try {
        testShortNamesAndPadding();
        testExtendedNames();
        testMalformedArchives();
        testMicrosoftLongNames();
        testMalformedMicrosoftLongNames();
        if (argc > 1) {
            std::ifstream input(argv[1], std::ios::binary);
            if (!input) throw std::runtime_error("cannot open archive fixture");
            std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            auto members = pcc::readArchive(bytes);
            require(members.size() == 2, "wrong real archive member count");
            require(members[0].name == "short.o" && members[0].bytes == "odd",
                    "incorrect real short member");
            require(members[1].name == "long-archive-member-name.o" && members[1].bytes == "even",
                    "incorrect real long member");
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
