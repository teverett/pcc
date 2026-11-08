// pcc - a compiler for pdp-c-compiler's C subset.
//
// Reads one or more C source files, parsed separately but merged into one
// program sharing a flat global/function namespace, then compiles the
// result via a Backend (see Codegen.h) and, unless --format=asm, pipes the
// output through the target's real assembler.

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#else
#include <sys/stat.h>
#include <sys/wait.h>
#endif

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "codegen/Codegen.h"
#include "codegen/i386/I386Assembler.h"
#include "ast/BuildAst.h"
#include "CLexer.h"
#include "CParser.h"
#include "CommonTokenStream.h"
#include "ir/IRBuilder.h"
#include "ir/IRPrinter.h"
#include "ir/IRValidator.h"
#include "optimizers/algebraic_simplification/AlgebraicSimplification.h"
#include "optimizers/constant_folding/ConstantFolding.h"
#include "optimizers/constant_propagation/ConstantPropagation.h"
#include "optimizers/dead_code_elimination/DeadCodeElimination.h"
#include "optimizers/dead_store_elimination/DeadStoreElimination.h"
#include "optimizers/identical_code_folding/IdenticalCodeFolding.h"
#include "optimizers/local_cse/LocalCSE.h"
#include "optimizers/unused_global_elimination/UnusedGlobalElimination.h"
#include "preprocessor/Preprocessor.h"
#include "codegen/pdp7/Pdp7Codegen.h"
#include "codegen/x86_8086/I8086Codegen.h"
#include "codegen/m6502/M6502Codegen.h"
#include "codegen/m65c02/M65C02Codegen.h"
#include "codegen/m68k/M68kCodegen.h"
#include "codegen/mc68328/MC68328Codegen.h"
#include "codegen/pdp11/Pdp11Codegen.h"
#include "codegen/i8080/I8080Codegen.h"
#include "codegen/z80/Z80Codegen.h"
#include "codegen/i8008/I8008Codegen.h"
#include "codegen/m16c/M16cCodegen.h"
#include "codegen/stm8/STM8Codegen.h"
#include "codegen/stm32/STM32Codegen.h"
#include "codegen/i386/I386Codegen.h"
#include "codegen/pic16/PIC16Codegen.h"
#include "codegen/pic18/PIC18Codegen.h"
#include "codegen/mips/MipsCodegen.h"
#include "codegen/armm/ArmMCodegen.h"
#include "codegen/mcs51/MCS51Codegen.h"
#include "codegen/arm/ArmCodegen.h"
#include "codegen/arm64/Arm64Codegen.h"
#include "codegen/arm64/Arm64Assembler.h"
#include "codegen/powerpc/PowerPCCodegen.h"
#include "codegen/sparc/SparcCodegen.h"
#include "fileformat/FileFormat.h"
#include "linker/Linker.h"
#include "linker/factory/I8086LinkerFactory.h"
#include "linker/factory/Pdp7LinkerFactory.h"
#include "linker/factory/Z80LinkerFactory.h"
#include "linker/factory/M6502LinkerFactory.h"
#include "linker/factory/M65C02LinkerFactory.h"
#include "linker/factory/I8080LinkerFactory.h"
#include "linker/factory/I8008LinkerFactory.h"
#include "linker/factory/STM8LinkerFactory.h"
#include "linker/factory/MC68328LinkerFactory.h"
#include "linker/factory/M68kLinkerFactory.h"
#include "linker/factory/Pdp11LinkerFactory.h"
#include "linker/factory/M16cLinkerFactory.h"
#include "linker/factory/I386LinkerFactory.h"
#include "linker/factory/ArmLinkerFactory.h"
#include "linker/factory/ArmMLinkerFactory.h"
#include "linker/factory/Arm64LinkerFactory.h"
#include "linker/factory/STM32LinkerFactory.h"
#include "linker/factory/MipsLinkerFactory.h"
#include "linker/factory/PowerPCLinkerFactory.h"
#include "linker/factory/SparcLinkerFactory.h"
#include "linker/factory/MCS51LinkerFactory.h"
#include "linker/factory/PIC16LinkerFactory.h"
#include "linker/factory/PIC18LinkerFactory.h"

namespace {

// ANTLR's default error listener only prints to stderr and keeps
// going; this one also lets main() know a syntax error happened at
// all, so it can refuse to compile a tree built from broken input.
class CountingErrorListener : public antlr4::BaseErrorListener {
   public:
    void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t, size_t, const std::string &,
                      std::exception_ptr) override {
        count++;
    }
    int count = 0;
};

void usage(const char *argv0) {
    std::cerr << "Usage: " << argv0
              << " [--target=pdp7|8086] [--hosted] [--format=asm|<target-specific>] [--out file] [--assembler path]\n"
                 "       [--optimize] [--dump-ir] [--dump-asm] file.c [file2.c ...]\n"
                 "   or: " << argv0
              << " --link [--target=pdp7|8086] [--hosted] --format=<target-native> [--out file] file1.o [file2.o ...]\n"
                 "\n"
                 "A non-coff --format takes exactly one .c file - real separate compilation\n"
                 "is now the only way to combine more than one: compile each file on its own\n"
                 "with --format=coff (one invocation, several files, each compiled completely\n"
                 "independently - one build/<name>.o per file), then combine every .o with\n"
                 "--link into the final program. For ELF/i386 Linux executables, --hosted\n"
                 "uses the system GNU-compatible linker and accepts -L<dir>, -l<name>, .a,\n"
                 "and .so inputs; --linker=<path> selects the linker (default: ld). For\n"
                 "32-bit Windows executables, use --hosted=windows --format=coff; choose\n"
                 "--windows-linker=mingw (default) or msvc to link .lib libraries. For\n"
                 "Apple Silicon macOS executables, use --hosted=macos --target=armv8a\n"
                 "--format=macho; this uses the system linker and libSystem.\n";
}

bool usesHostedLibc(const std::vector<std::string> &inputs) {
    auto isLibcFile = [](const std::string &name) {
        return name == "libc.a" || name == "libc.so" || name.compare(0, 8, "libc.so.") == 0;
    };
    for (size_t i = 0; i < inputs.size(); ++i) {
        const std::string &input = inputs[i];
        if (input == "-L") {
            if (i + 1 < inputs.size()) ++i;
        } else if (input == "-l") {
            if (i + 1 < inputs.size()) {
                const std::string &name = inputs[++i];
                if (name == "c" || (name.size() > 1 && name[0] == ':' && isLibcFile(name.substr(1)))) {
                    return true;
                }
            }
        } else if (input.compare(0, 2, "-l") == 0) {
            std::string name = input.substr(2);
            if (name == "c" || (name.size() > 1 && name[0] == ':' && isLibcFile(name.substr(1)))) {
                return true;
            }
        } else if (input.compare(0, 2, "-L") != 0 &&
                   isLibcFile(std::filesystem::path(input).filename().string())) {
            return true;
        }
    }
    return false;
}

std::string writeHostedStartupObject(bool useLibc) {
    const std::string libcAssembly =
        "org 0\n"
        "_start:\n"
        "    mov ebp,0\n"
        "    pop esi\n"
        "    mov ecx,esp\n"
        "    and esp,-16\n"
        "    mov eax,0\n"
        "    push eax\n"
        "    push esp\n"
        "    push edx\n"
        "    push eax\n"
        "    push eax\n"
        "    push ecx\n"
        "    push esi\n"
        "    mov eax,main\n"
        "    push eax\n"
        "    call __libc_start_main\n"
        "    hlt\n";
    const std::string minimalAssembly =
        "org 0\n"
        "_start:\n"
        "    pop eax\n"
        "    mov edx,esp\n"
        "    sub esp,4\n"
        "    push edx\n"
        "    push eax\n"
        "    call main\n"
        "    add esp,12\n"
        "    mov ebx,eax\n"
        "    mov eax,1\n"
        "    db 205,128\n";
    pcc::ObjectFile object = pcc::assembleI386Object(useLibc ? libcAssembly : minimalAssembly);
    auto writer = pcc::makeFileFormatWriter(pcc::FileFormatKind::Elf);
    std::string bytes = writer->write(object);

    std::string pattern = (std::filesystem::temp_directory_path() / "pcc_startup_XXXXXX").string();
    std::vector<char> path(pattern.begin(), pattern.end());
    path.push_back('\0');
    int fd = mkstemp(path.data());
    if (fd < 0) throw std::runtime_error("could not create hosted-link startup object");
    close(fd);
    std::ofstream out(path.data(), std::ios::binary | std::ios::trunc);
    if (!out) {
        std::remove(path.data());
        throw std::runtime_error("could not write hosted-link startup object");
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        out.close();
        std::remove(path.data());
        throw std::runtime_error("could not write hosted-link startup object");
    }
    return path.data();
}

std::string writeHostedWindowsStartupObject() {
    const std::string assembly =
        "org 0\n"
        "_start:\n"
        "    mov eax,0\n"
        "    push eax\n"
        "    push eax\n"
        "    call _main\n"
        "    add esp,8\n"
        "    push eax\n"
        "    call pcc_exit_process\n";
    pcc::ObjectFile object = pcc::assembleI386Object(assembly);
    object.machineType = 0x014C;
    for (auto &symbol : object.symbols) {
        if (symbol.name == "pcc_exit_process") symbol.name = "_ExitProcess@4";
    }
    for (auto &relocation : object.relocations) {
        if (relocation.symbolName == "pcc_exit_process") relocation.symbolName = "_ExitProcess@4";
    }
    auto writer = pcc::makeFileFormatWriter(pcc::FileFormatKind::Coff);
    std::string bytes = writer->write(object);

    std::string pattern = (std::filesystem::temp_directory_path() / "pcc_win_startup_XXXXXX").string();
    std::vector<char> path(pattern.begin(), pattern.end());
    path.push_back('\0');
    int fd = mkstemp(path.data());
    if (fd < 0) throw std::runtime_error("could not create hosted Windows startup object");
    close(fd);
    std::ofstream out(path.data(), std::ios::binary | std::ios::trunc);
    if (!out) {
        std::remove(path.data());
        throw std::runtime_error("could not write hosted Windows startup object");
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        out.close();
        std::remove(path.data());
        throw std::runtime_error("could not write hosted Windows startup object");
    }
    return path.data();
}

std::string writeHostedMacOSStartupObject() {
    const std::string assembly =
        "org 0\n"
        "_start:\n"
        "    bl _main\n"
        "    bl _exit\n";
    pcc::ObjectFile object = pcc::assembleArm64Object(assembly, /*darwinHosted=*/true);
    auto writer = pcc::makeFileFormatWriter(pcc::FileFormatKind::MachO);
    std::string bytes = writer->write(object);

    std::string pattern = (std::filesystem::temp_directory_path() / "pcc_macos_startup_XXXXXX").string();
    std::vector<char> path(pattern.begin(), pattern.end());
    path.push_back('\0');
    int fd = mkstemp(path.data());
    if (fd < 0) throw std::runtime_error("could not create hosted macOS startup object");
    close(fd);
    std::ofstream out(path.data(), std::ios::binary | std::ios::trunc);
    if (!out) {
        std::remove(path.data());
        throw std::runtime_error("could not write hosted macOS startup object");
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        out.close();
        std::remove(path.data());
        throw std::runtime_error("could not write hosted macOS startup object");
    }
    return path.data();
}

int runHostedLinker(const std::vector<std::string> &inputs, const std::string &linker,
                    const std::string &outPath) {
#ifdef _WIN32
    (void)inputs;
    (void)linker;
    (void)outPath;
    throw std::runtime_error("hosted ELF/i386 linking requires a POSIX ELF linker");
#else
    std::string startup = writeHostedStartupObject(usesHostedLibc(inputs));
    std::filesystem::path destination(outPath);
    std::filesystem::path parent = destination.parent_path();
    if (parent.empty()) parent = ".";
    std::string outputPattern = (parent / ".pcc-link-XXXXXX").string();
    std::vector<char> outputPath(outputPattern.begin(), outputPattern.end());
    outputPath.push_back('\0');
    int outputFd = mkstemp(outputPath.data());
    if (outputFd < 0) {
        std::remove(startup.c_str());
        throw std::runtime_error("could not create a temporary hosted-link output");
    }
    close(outputFd);
    std::string temporaryOutput(outputPath.data());
    std::vector<std::string> args = {linker, "-m", "elf_i386", "-e", "_start", "-o", temporaryOutput,
                                     "--dynamic-linker", "/lib/ld-linux.so.2", startup};
    args.insert(args.end(), inputs.begin(), inputs.end());

    pid_t pid = fork();
    if (pid < 0) {
        std::remove(startup.c_str());
        std::remove(temporaryOutput.c_str());
        throw std::runtime_error("could not start hosted ELF linker: " + std::string(std::strerror(errno)));
    }
    if (pid == 0) {
        std::vector<char *> argv;
        argv.reserve(args.size() + 1);
        for (std::string &arg : args) argv.push_back(arg.data());
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        std::perror("pcc: hosted ELF linker");
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        std::remove(startup.c_str());
        std::remove(temporaryOutput.c_str());
        throw std::runtime_error("could not wait for hosted ELF linker: " + std::string(std::strerror(errno)));
    }
    std::remove(startup.c_str());
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::remove(temporaryOutput.c_str());
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    if (std::rename(temporaryOutput.c_str(), outPath.c_str()) != 0) {
        std::remove(temporaryOutput.c_str());
        throw std::runtime_error("could not move hosted-link output to '" + outPath + "'");
    }
    return 0;
#endif
}

int runHostedMacOSLinker(const std::vector<std::string> &inputs, const std::string &linker,
                         const std::string &outPath) {
#ifdef _WIN32
    (void)inputs;
    (void)linker;
    (void)outPath;
    throw std::runtime_error("hosted macOS/arm64 linking requires Apple's macOS linker");
#else
    FILE *sdkPipe = popen("xcrun --sdk macosx --show-sdk-path", "r");
    if (!sdkPipe) throw std::runtime_error("could not locate the macOS SDK with xcrun");
    char sdkBuffer[4096];
    std::string sdkPath;
    while (fgets(sdkBuffer, sizeof(sdkBuffer), sdkPipe)) sdkPath += sdkBuffer;
    int sdkStatus = pclose(sdkPipe);
    while (!sdkPath.empty() && (sdkPath.back() == '\n' || sdkPath.back() == '\r')) sdkPath.pop_back();
    if (sdkStatus != 0 || sdkPath.empty()) {
        throw std::runtime_error("could not locate the macOS SDK with xcrun");
    }

    std::string startup = writeHostedMacOSStartupObject();
    std::filesystem::path destination(outPath);
    std::filesystem::path parent = destination.parent_path();
    if (parent.empty()) parent = ".";
    std::string outputPattern = (parent / ".pcc-macos-link-XXXXXX").string();
    std::vector<char> outputPath(outputPattern.begin(), outputPattern.end());
    outputPath.push_back('\0');
    int outputFd = mkstemp(outputPath.data());
    if (outputFd < 0) {
        std::remove(startup.c_str());
        throw std::runtime_error("could not create a temporary hosted macOS output");
    }
    close(outputFd);
    std::string temporaryOutput(outputPath.data());
    std::vector<std::string> args = {
        linker, "-arch", "arm64", "-syslibroot", sdkPath, "-platform_version", "macos", "11.0", "15.0",
        "-e", "_start", "-o", temporaryOutput, startup};
    args.insert(args.end(), inputs.begin(), inputs.end());
    args.push_back("-lSystem");

    pid_t pid = fork();
    if (pid < 0) {
        std::remove(startup.c_str());
        std::remove(temporaryOutput.c_str());
        throw std::runtime_error("could not start hosted macOS linker: " + std::string(std::strerror(errno)));
    }
    if (pid == 0) {
        std::vector<char *> argv;
        argv.reserve(args.size() + 1);
        for (std::string &arg : args) argv.push_back(arg.data());
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        std::perror("pcc: hosted macOS linker");
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        std::remove(startup.c_str());
        std::remove(temporaryOutput.c_str());
        throw std::runtime_error("could not wait for hosted macOS linker: " + std::string(std::strerror(errno)));
    }
    std::remove(startup.c_str());
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::remove(temporaryOutput.c_str());
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    chmod(temporaryOutput.c_str(), 0755);
    if (std::rename(temporaryOutput.c_str(), outPath.c_str()) != 0) {
        std::remove(temporaryOutput.c_str());
        throw std::runtime_error("could not move hosted macOS output to '" + outPath + "'");
    }
    return 0;
#endif
}

int runHostedWindowsLinker(const std::vector<std::string> &inputs, const std::string &linker,
                           const std::string &linkerKind, const std::string &outPath) {
    std::string startup = writeHostedWindowsStartupObject();
    std::filesystem::path destination(outPath);
    std::filesystem::path parent = destination.parent_path();
    if (parent.empty()) parent = ".";
    std::string outputPattern = (parent / ".pcc-win-link-XXXXXX").string();
    std::vector<char> outputPath(outputPattern.begin(), outputPattern.end());
    outputPath.push_back('\0');
    int outputFd = mkstemp(outputPath.data());
    if (outputFd < 0) {
        std::remove(startup.c_str());
        throw std::runtime_error("could not create a temporary hosted Windows output");
    }
    close(outputFd);
    std::string temporaryOutput(outputPath.data());

    std::vector<std::string> args;
    if (linkerKind == "msvc") {
        args = {linker, "/nologo", "/machine:x86", "/subsystem:console", "/entry:start",
                "/out:" + temporaryOutput, startup};
        for (size_t i = 0; i < inputs.size(); ++i) {
            const std::string &input = inputs[i];
            if (input == "-L" && i + 1 < inputs.size()) {
                args.push_back("/LIBPATH:" + inputs[++i]);
            } else if (input == "-l" && i + 1 < inputs.size()) {
                args.push_back(inputs[++i] + ".lib");
            } else if (input.compare(0, 2, "-L") == 0) {
                args.push_back("/LIBPATH:" + input.substr(2));
            } else if (input.compare(0, 2, "-l") == 0) {
                args.push_back(input.substr(2) + ".lib");
            } else {
                args.push_back(input);
            }
        }
        args.push_back("kernel32.lib");
    } else {
        args = {linker, "-nostdlib", "-Wl,--subsystem,console", "-Wl,-e,_start",
                "-o", temporaryOutput, startup};
        std::vector<std::filesystem::path> libraryPaths{"."};
        for (size_t i = 0; i < inputs.size(); ++i) {
            std::string input = inputs[i];
            if (input == "-L" && i + 1 < inputs.size()) {
                libraryPaths.emplace_back(inputs[i + 1]);
                args.push_back(input);
                args.push_back(inputs[++i]);
            } else if (input.compare(0, 2, "-L") == 0) {
                libraryPaths.emplace_back(input.substr(2));
                args.push_back(input);
            } else {
                std::string libraryName;
                bool separateLibrary = input == "-l" && i + 1 < inputs.size();
                if (separateLibrary) libraryName = inputs[++i];
                else if (input.compare(0, 2, "-l") == 0) libraryName = input.substr(2);
                if (!libraryName.empty()) {
                    std::filesystem::path found;
                    for (const auto &directory : libraryPaths) {
                        std::filesystem::path candidate = directory / (libraryName + ".lib");
                        std::error_code error;
                        if (std::filesystem::is_regular_file(candidate, error)) {
                            found = std::move(candidate);
                            break;
                        }
                    }
                    if (!found.empty()) args.push_back(found.string());
                    else if (separateLibrary) {
                        args.push_back("-l");
                        args.push_back(libraryName);
                    } else {
                        args.push_back(input);
                    }
                } else {
                    args.push_back(input);
                }
            }
        }
        args.push_back("-lkernel32");
    }

    int status = -1;
#ifdef _WIN32
    std::vector<const char *> argv;
    argv.reserve(args.size() + 1);
    for (const std::string &arg : args) argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    status = _spawnvp(_P_WAIT, argv[0], argv.data());
    if (status < 0) {
        std::remove(startup.c_str());
        std::remove(temporaryOutput.c_str());
        throw std::runtime_error("could not start hosted Windows linker '" + linker + "'");
    }
#else
    pid_t pid = fork();
    if (pid < 0) {
        std::remove(startup.c_str());
        std::remove(temporaryOutput.c_str());
        throw std::runtime_error("could not start hosted Windows linker: " + std::string(std::strerror(errno)));
    }
    if (pid == 0) {
        std::vector<char *> argv;
        argv.reserve(args.size() + 1);
        for (std::string &arg : args) argv.push_back(arg.data());
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        std::perror("pcc: hosted Windows linker");
        _exit(127);
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        std::remove(startup.c_str());
        std::remove(temporaryOutput.c_str());
        throw std::runtime_error("could not wait for hosted Windows linker: " + std::string(std::strerror(errno)));
    }
    if (!WIFEXITED(status)) status = 1;
    else status = WEXITSTATUS(status);
#endif
    std::remove(startup.c_str());
    if (status != 0) {
        std::remove(temporaryOutput.c_str());
        return status;
    }
    if (std::rename(temporaryOutput.c_str(), outPath.c_str()) != 0) {
        std::remove(temporaryOutput.c_str());
        throw std::runtime_error("could not move hosted Windows output to '" + outPath + "'");
    }
    return 0;
}

// Strips directory and ".c" (e.g. "src/s5.c" -> "s5") for naming
// --dump-ir/--dump-asm output files.
std::string baseName(const std::string &srcPath) {
    size_t slash = srcPath.find_last_of('/');
    std::string base = (slash == std::string::npos) ? srcPath : srcPath.substr(slash + 1);
    if (base.size() > 2 && base.substr(base.size() - 2) == ".c") base = base.substr(0, base.size() - 2);
    return base;
}

// MinGW's mkdir() (no <sys/stat.h>) takes no mode argument, so every
// caller goes through this rather than calling mkdir() directly.
void makeDir(const std::string &path) {
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}

void writeToBuildDir(const std::string &name, const std::string &content, bool binary = false) {
    makeDir("build");  // any failure here surfaces via the ofstream below instead
    std::ofstream out("build/" + name, binary ? std::ios::binary : std::ios::out);
    if (!out) {
        std::cerr << "warning: could not write build/" << name << "\n";
        return;
    }
    out << content;
}

// Several --target names below are deliberate, software-identical aliases
// for another target rather than distinct backends (8088->8086, r4000->
// r10000, cortex-m4/m7->cortex-m3, 8031/8032/8051/8052 all share one
// backend): each pair's real hardware differences are invisible to any
// instruction this compiler emits, so there's nothing for a separate
// Codegen to do differently.
std::unique_ptr<pcc::Backend> makeBackend(const std::string &target) {
    if (target == "pdp7") return std::make_unique<pcc::Pdp7Codegen>();
    if (target == "8086" || target == "8088") return std::make_unique<pcc::I8086Codegen>();
    if (target == "6502") return std::make_unique<pcc::M6502Codegen>();
    if (target == "65c02") return std::make_unique<pcc::M65C02Codegen>();
    if (target == "68000") return std::make_unique<pcc::M68kCodegen>();
    if (target == "mc68328") return std::make_unique<pcc::MC68328Codegen>();
    if (target == "pdp11") return std::make_unique<pcc::Pdp11Codegen>();
    if (target == "8080") return std::make_unique<pcc::I8080Codegen>();
    if (target == "z80") return std::make_unique<pcc::Z80Codegen>();
    if (target == "8008") return std::make_unique<pcc::I8008Codegen>();
    if (target == "stm8") return std::make_unique<pcc::STM8Codegen>();
    if (target == "stm32") return std::make_unique<pcc::STM32Codegen>();
    if (target == "i386") return std::make_unique<pcc::I386Codegen>();
    if (target == "m16c") return std::make_unique<pcc::M16cCodegen>();
    if (target == "pic16") return std::make_unique<pcc::PIC16Codegen>();
    if (target == "pic18") return std::make_unique<pcc::PIC18Codegen>();
    if (target == "r10000" || target == "r4000") return std::make_unique<pcc::MipsCodegen>();
    if (target == "cortex-m3" || target == "cortex-m4" || target == "cortex-m7") {
        return std::make_unique<pcc::ArmMCodegen>();
    }
    if (target == "8051" || target == "8052" || target == "8031" || target == "8032") {
        return std::make_unique<pcc::MCS51Codegen>();
    }
    // armv7a (32-bit AArch32) and armv8a (64-bit AArch64) are genuinely
    // different instruction sets, each with its own Codegen.
    if (target == "armv7a") return std::make_unique<pcc::ArmCodegen>();
    if (target == "armv8a") return std::make_unique<pcc::Arm64Codegen>();
    if (target == "powerpc") return std::make_unique<pcc::PowerPCCodegen>();
    if (target == "sparc") return std::make_unique<pcc::SparcCodegen>();
    return nullptr;
}

std::unique_ptr<pcc::TargetLinker> makeLinker(const std::string &target) {
    if (target == "pdp7") return pcc::makePdp7Linker();
    if (target == "8086" || target == "8088") return pcc::makeI8086Linker();
    if (target == "z80") return pcc::makeZ80Linker();
    if (target == "6502") return pcc::makeM6502Linker();
    if (target == "65c02") return pcc::makeM65C02Linker();
    if (target == "8080") return pcc::makeI8080Linker();
    if (target == "8008") return pcc::makeI8008Linker();
    if (target == "stm8") return pcc::makeSTM8Linker();
    if (target == "mc68328") return pcc::makeMC68328Linker();
    if (target == "68000") return pcc::makeM68kLinker();
    if (target == "pdp11") return pcc::makePdp11Linker();
    if (target == "m16c") return pcc::makeM16cLinker();
    if (target == "i386") return pcc::makeI386Linker();
    if (target == "armv7a") return pcc::makeArmLinker();
    if (target == "armv8a") return pcc::makeArm64Linker();
    if (target == "cortex-m3" || target == "cortex-m4" || target == "cortex-m7") return pcc::makeArmMLinker();
    if (target == "stm32") return pcc::makeSTM32Linker();
    if (target == "r10000" || target == "r4000") return pcc::makeMipsLinker();
    if (target == "powerpc") return pcc::makePowerPCLinker();
    if (target == "sparc") return pcc::makeSparcLinker();
    if (target == "8051" || target == "8052" || target == "8031" || target == "8032") return pcc::makeMCS51Linker();
    if (target == "pic16") return pcc::makePIC16Linker();
    if (target == "pic18") return pcc::makePIC18Linker();
    return nullptr;
}


// Parses one file into its own Program, then appends its functions/globals
// onto `combined`; name resolution (IRBuilder) runs once, afterward, over
// the whole concatenated list, so no file needs to declare another's
// function first.
void parseAndMergeFile(const std::string &srcPath, const std::string &source, pcc::Program &combined) {
    CountingErrorListener errors;

    antlr4::ANTLRInputStream input(source);
    CLexer lexer(&input);
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errors);

    antlr4::CommonTokenStream tokens(&lexer);
    CParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&errors);

    CParser::ProgramContext *tree = parser.program();
    if (errors.count > 0) throw std::runtime_error(srcPath + ": syntax errors, not compiling");

    pcc::Program fileProg = pcc::buildProgram(tree);
    for (auto &fn : fileProg.functions) {
        fn.sourceFile = srcPath;
        combined.functions.push_back(std::move(fn));
    }
    for (auto &g : fileProg.globals) {
        g.sourceFile = srcPath;
        combined.globals.push_back(std::move(g));
    }
}

// Compiles exactly one source file with no visibility into any other file
// on this invocation. `allowExternal` (true only for --format=coff) is
// threaded through to both buildIR() and Backend::generate()'s objectMode.
std::string compileOneFile(const std::string &srcPath, const std::map<std::string, std::string> &predefinedMacros,
                             pcc::Backend &backend, bool allowExternal, bool optimize, bool dumpIr, bool dumpAsm) {
    pcc::Program prog;
    parseAndMergeFile(srcPath, pcc::preprocess(srcPath, predefinedMacros), prog);

    pcc::ir::Program irProg = pcc::buildIR(prog, backend.typeWidths(), allowExternal);

    if (optimize) {
        // Constant propagation first: substituting a Var read with its
        // known literal value both feeds ConstantFolding new
        // literal-literal expressions to fold and, once a local's only
        // reads are gone, feeds DeadCodeElimination a now-unused
        // declaration to drop.
        pcc::ConstantPropagation cprop;
        cprop.run(irProg, srcPath);
        // Constant folding next, so DeadCodeElimination can reason about
        // initializers it collapses to a side-effect-free constant.
        pcc::ConstantFolding cf;
        cf.run(irProg, srcPath);
        // Strength reduction only rewrites an op whose other operand isn't
        // a literal, so ordering after constant folding loses nothing.
        pcc::AlgebraicSimplification algsimp;
        algsimp.run(irProg, srcPath);
        // Local CSE after the above so it matches each pass's canonicalized
        // form (e.g. a strength-reduced `x << 1`, not the original `x * 2`).
        pcc::LocalCSE cse;
        cse.run(irProg, srcPath);
        // Before DeadCodeElimination, so a LocalDecl this pass strips down
        // to a bare, unread declaration looks unused to it too.
        pcc::DeadStoreElimination dse;
        dse.run(irProg, srcPath);
        pcc::DeadCodeElimination dce;
        dce.run(irProg, srcPath);
        // LocalCSE may have introduced new locals after buildIR() already
        // assigned frame offsets once.
        pcc::recomputeFrameLayout(irProg, backend.typeWidths());
        // Only when this compile sees the entire program: dropping a
        // function/global's own definition is only safe once every
        // reference to it is known, which --format=coff's separate
        // compilation doesn't guarantee.
        if (!allowExternal) {
            pcc::IdenticalCodeFolding icf;
            icf.run(irProg, srcPath);
            pcc::UnusedGlobalElimination uge;
            uge.run(irProg, srcPath);
        }
    }

    pcc::validateIRSyntax(irProg);

    if (dumpIr) writeToBuildDir(baseName(srcPath) + ".ir", pcc::printIR(irProg, srcPath));

    std::string asmText = backend.generate(irProg, /*objectMode=*/allowExternal);
    if (dumpAsm) {
        std::map<std::string, std::string> perFile = backend.perFileAsm();
        auto it = perFile.find(srcPath);
        if (it != perFile.end()) writeToBuildDir(baseName(srcPath) + backend.sourceExtension(), it->second);
    }
    return asmText;
}

// Assembles `asmText` into `format`; "asm" is a no-op. Prefers the
// backend's in-process assembler, falling back to its external assembler
// tool via a temp file pair.
std::string assembleOne(const pcc::Backend &backend, const std::string &asmText, const std::string &format,
                          const std::string &assemblerPath) {
    if (format == "asm") return asmText;

    if (std::optional<std::string> result = backend.assembleInProcess(asmText, format)) return *result;

    // mkstemp() (not mkstemps(), MinGW lacks it) can't give the input file
    // its required extension directly, so it's added via a rename after.
    std::string tmpDir = std::filesystem::temp_directory_path().string();
    std::string tmpPathTemplate = tmpDir + "/pcc_XXXXXX";
    std::vector<char> tmpPath(tmpPathTemplate.begin(), tmpPathTemplate.end());
    tmpPath.push_back('\0');
    int fd = mkstemp(tmpPath.data());
    if (fd < 0) throw std::runtime_error("could not create a temporary file for the generated assembly");
    close(fd);
    std::string tmpPathStr = std::string(tmpPath.data()) + backend.sourceExtension();
    if (std::rename(tmpPath.data(), tmpPathStr.c_str()) != 0) {
        std::remove(tmpPath.data());
        throw std::runtime_error("could not create a temporary file for the generated assembly");
    }
    {
        std::ofstream tmp(tmpPathStr);
        tmp << asmText;
    }

    std::string outTmpPathTemplate = tmpDir + "/pcc_out_XXXXXX";
    std::vector<char> outTmpPath(outTmpPathTemplate.begin(), outTmpPathTemplate.end());
    outTmpPath.push_back('\0');
    int outFd = mkstemp(outTmpPath.data());
    if (outFd < 0) {
        std::remove(tmpPathStr.c_str());
        throw std::runtime_error("could not create a temporary output file");
    }
    std::string outTmpPathStr(outTmpPath.data());
    close(outFd);

    std::string cmd = backend.buildAssembleCommand(assemblerPath, format, tmpPathStr, outTmpPathStr);
    int rc = std::system(cmd.c_str());
    std::remove(tmpPathStr.c_str());
    if (rc != 0) {
        std::remove(outTmpPathStr.c_str());
        throw std::runtime_error("assembling with '" + assemblerPath +
                                   "' failed (pass --assembler=<path> if it's not on PATH / at the default location)");
    }
    std::ifstream outFile(outTmpPathStr, std::ios::binary);
    std::ostringstream ss;
    ss << outFile.rdbuf();
    std::remove(outTmpPathStr.c_str());
    return ss.str();
}

}  // namespace

void writeResult(const std::string &outPath, const std::string &content) {
    if (outPath.empty()) {
        std::cout << content;
    } else {
        std::ofstream out(outPath, std::ios::binary);
        out << content;
    }
}

int main(int argc, char **argv) {
    std::string target = "pdp7";
    std::string format = "asm";
    std::string outPath;
    std::string assemblerPath;
    bool assemblerPathSet = false;
    std::string systemLinker = "ld";
    bool systemLinkerSet = false;
    std::vector<std::string> srcPaths;
    std::vector<std::string> systemLinkInputs;
    bool hosted = false;
    bool windowsHosted = false;
    bool macosHosted = false;
    std::string windowsLinkerKind = "mingw";
    bool windowsLinkerKindSet = false;
    bool librarySearchOptions = false;
    bool optimize = false;
    bool dumpIr = false;
    bool dumpAsm = false;
    bool link = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto takeValue = [&](const std::string &flag) -> std::string {
            if (arg.size() > flag.size() && arg.compare(0, flag.size(), flag) == 0) return arg.substr(flag.size());
            return "";
        };
        std::string v;
        if (!(v = takeValue("--target=")).empty()) {
            target = v;
        } else if (!(v = takeValue("--format=")).empty()) {
            format = v;
        } else if (!(v = takeValue("--out=")).empty()) {
            outPath = v;
        } else if (!(v = takeValue("--assembler=")).empty()) {
            assemblerPath = v;
            assemblerPathSet = true;
        } else if (!(v = takeValue("--linker=")).empty()) {
            systemLinker = v;
            systemLinkerSet = true;
        } else if (!(v = takeValue("--windows-linker=")).empty()) {
            windowsLinkerKind = v;
            windowsLinkerKindSet = true;
        } else if (arg == "--hosted") {
            hosted = true;
        } else if (arg == "--hosted=windows") {
            hosted = true;
            windowsHosted = true;
        } else if (arg == "--hosted=macos") {
            hosted = true;
            macosHosted = true;
        } else if (arg == "-L" || arg == "-l") {
            if (i + 1 >= argc) {
                std::cerr << arg << " requires a value\n";
                return 1;
            }
            systemLinkInputs.push_back(arg);
            systemLinkInputs.push_back(argv[++i]);
            librarySearchOptions = true;
        } else if (arg.size() > 2 && (arg.compare(0, 2, "-L") == 0 || arg.compare(0, 2, "-l") == 0)) {
            systemLinkInputs.push_back(arg);
            librarySearchOptions = true;
        } else if (arg == "--optimize") {
            optimize = true;
        } else if (arg == "--dump-ir") {
            dumpIr = true;
        } else if (arg == "--dump-asm") {
            dumpAsm = true;
        } else if (arg == "--link") {
            link = true;
        } else if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            return 0;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "unknown option: " << arg << "\n";
            usage(argv[0]);
            return 1;
        } else {
            srcPaths.push_back(arg);
            systemLinkInputs.push_back(arg);
        }
    }

    if (srcPaths.empty()) {
        usage(argv[0]);
        return 1;
    }

    if (hosted && !windowsHosted && !macosHosted && (target != "i386" || format != "elf")) {
        std::cerr << "--hosted requires --target=i386 and --format=elf\n";
        return 1;
    }
    if (macosHosted && (target != "armv8a" || format != "macho")) {
        std::cerr << "--hosted=macos requires --target=armv8a and --format=macho\n";
        return 1;
    }
    if (windowsHosted && (target != "i386" || format != "coff")) {
        std::cerr << "--hosted=windows requires --target=i386 and --format=coff\n";
        return 1;
    }
    if (windowsLinkerKind != "msvc" && windowsLinkerKind != "mingw") {
        std::cerr << "--windows-linker must be 'msvc' or 'mingw'\n";
        return 1;
    }
    if (windowsLinkerKindSet && !windowsHosted) {
        std::cerr << "--windows-linker is only valid with --hosted=windows\n";
        return 1;
    }
    if (windowsLinkerKindSet && !link) {
        std::cerr << "--windows-linker requires --link\n";
        return 1;
    }
    if (!link && (hosted || systemLinkerSet || librarySearchOptions)) {
        if (librarySearchOptions) {
            std::cerr << "-L/-l are only valid with --link --hosted\n";
            return 1;
        }
        if (systemLinkerSet) {
            std::cerr << "--linker is only valid with --link --hosted\n";
            return 1;
        }
    }
    if (link && (librarySearchOptions || systemLinkerSet) && !hosted) {
        std::cerr << "-L/-l/--linker require --link --hosted\n";
        return 1;
    }

    // --link mode: merging .o files (TargetLinker) and writing the result
    // in a given format (FileFormatWriter) are decoupled; `target` picks
    // the first, `format` the second.
    if (link) {
        if (hosted) {
            if (srcPaths.empty()) {
                std::cerr << "--link --hosted requires at least one object or library input\n";
                return 1;
            }
            try {
                std::string output = outPath.empty() ? "a.out" : outPath;
                std::string linker = systemLinker;
                if (windowsHosted && !systemLinkerSet) {
                    linker = windowsLinkerKind == "msvc" ? "link.exe" : "i686-w64-mingw32-gcc";
                }
                int rc = windowsHosted
                             ? runHostedWindowsLinker(systemLinkInputs, linker, windowsLinkerKind, output)
                             : macosHosted ? runHostedMacOSLinker(systemLinkInputs, linker, output)
                                           : runHostedLinker(systemLinkInputs, linker, output);
                if (rc != 0) {
                    std::cerr << "hosted linker '" << linker << "' failed\n";
                    return 1;
                }
            } catch (const std::exception &e) {
                std::cerr << e.what() << "\n";
                return 1;
            }
            return 0;
        }
        std::unique_ptr<pcc::TargetLinker> linker = makeLinker(target);
        if (!linker) {
            std::cerr << "unknown --target '" << target << "' (known targets: pdp7, 8086, 8088, 6502, 65c02, 68000, mc68328, pdp11, 8080, z80, 8008, stm8, stm32, i386, m16c, pic16, pic18, r10000, r4000, cortex-m3, cortex-m4, cortex-m7, 8051, 8052, 8031, 8032, armv7a, armv8a, powerpc, sparc)\n";
            return 1;
        }
        std::vector<std::string> linkFormats = linker->outputFormats();
        bool formatOk = false;
        for (const auto &f : linkFormats) formatOk = formatOk || format == f;
        if (!formatOk) {
            std::cerr << "unknown --format '" << format << "' for --link --target=" << target << " (valid:";
            for (const auto &f : linkFormats) std::cerr << " " << f;
            std::cerr << ")\n";
            return 1;
        }
        auto outKind = pcc::fileFormatKindFromName(format);
        // formatOk above already confirmed format names a real FileFormatKind.
        std::unique_ptr<pcc::FileFormatWriter> writer = pcc::makeFileFormatWriter(*outKind);
        try {
            std::vector<pcc::ObjectFile> objects;
            for (const std::string &path : srcPaths) {
                std::ifstream in(path, std::ios::binary);
                if (!in) throw std::runtime_error("could not read object file '" + path + "'");
                std::ostringstream ss;
                ss << in.rdbuf();
                std::string bytes = ss.str();
                // Sniffed from leading bytes, not assumed to be COFF, so
                // objects compiled with different --format choices can
                // still be linked together.
                pcc::FileFormatKind kind = pcc::detectFileFormat(bytes);
                objects.push_back(pcc::makeFileFormatReader(kind)->read(bytes));
            }
            pcc::ObjectFile linked = linker->merge(objects);
            writeResult(outPath, writer->write(linked));
        } catch (const std::exception &e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    // --- ordinary compile mode. ---
    std::unique_ptr<pcc::Backend> backend = makeBackend(target);
    if (!backend) {
        std::cerr << "unknown --target '" << target << "' (known targets: pdp7, 8086, 8088, 6502, 65c02, 68000, mc68328, pdp11, 8080, z80, 8008, stm8, stm32, i386, m16c, pic16, pic18, r10000, r4000, cortex-m3, cortex-m4, cortex-m7, 8051, 8052, 8031, 8032, armv7a, armv8a, powerpc, sparc)\n";
        return 1;
    }
    if (hosted) {
        auto *i386 = dynamic_cast<pcc::I386Codegen *>(backend.get());
        if (i386) {
            i386->setHostedAbi(true);
            i386->setWindowsAbi(windowsHosted);
        }
        if (auto *arm64 = dynamic_cast<pcc::Arm64Codegen *>(backend.get())) arm64->setHostedAbi(macosHosted);
    }
    if (!assemblerPathSet) assemblerPath = backend->defaultAssemblerPath();

    std::vector<std::string> assembledFormats = backend->assembledFormats();
    bool formatOk = format == "asm";
    for (const auto &f : assembledFormats) formatOk = formatOk || format == f;
    if (!formatOk) {
        std::cerr << "unknown --format '" << format << "' for --target=" << target << " (valid: asm";
        for (const auto &f : assembledFormats) std::cerr << ", " << f;
        std::cerr << ")\n";
        return 1;
    }

    // Real separate compilation is the only way to combine more than one
    // .c file; any other format takes exactly one source file.
    if (!backend->isObjectFormat(format) && srcPaths.size() != 1) {
        std::cerr << "--format=" << format << " takes exactly one .c file (got " << srcPaths.size()
                   << ") - compile each file as an object format, then --link them together; see --help\n";
        return 1;
    }
    if (backend->isObjectFormat(format) && srcPaths.size() > 1 && !outPath.empty()) {
        std::cerr << "--out can't be used with --format=" << format
                   << " and more than one input file - each file gets its "
                      "own build/<name>.o\n";
        return 1;
    }

    // Each target predefines its own macro (e.g. __PDP7__) for
    // #ifdef-style conditional code. An alias target (8088, 65c02,
    // mc68328, r4000, cortex-m4/m7, 8051/8052/8031/8032's siblings)
    // defines both its base target's macro and its own, so source gated
    // on either the generic or the specific name keeps working.
    std::map<std::string, std::string> predefinedMacros;
    if (target == "pdp7") {
        predefinedMacros["__PDP7__"] = "1";
    } else if (target == "8086") {
        predefinedMacros["__8086__"] = "1";
    } else if (target == "8088") {
        predefinedMacros["__8086__"] = "1";
        predefinedMacros["__8088__"] = "1";
    } else if (target == "6502") {
        predefinedMacros["__6502__"] = "1";
    } else if (target == "65c02") {
        predefinedMacros["__6502__"] = "1";
        predefinedMacros["__65C02__"] = "1";
    } else if (target == "68000") {
        predefinedMacros["__68000__"] = "1";
    } else if (target == "mc68328") {
        predefinedMacros["__68000__"] = "1";
        predefinedMacros["__MC68328__"] = "1";
    } else if (target == "pdp11") {
        predefinedMacros["__PDP11__"] = "1";
    } else if (target == "8080") {
        predefinedMacros["__8080__"] = "1";
    } else if (target == "z80") {
        predefinedMacros["__Z80__"] = "1";
    } else if (target == "8008") {
        predefinedMacros["__8008__"] = "1";
    } else if (target == "stm8") {
        predefinedMacros["__STM8__"] = "1";
    } else if (target == "stm32") {
        predefinedMacros["__STM32__"] = "1";
    } else if (target == "i386") {
        predefinedMacros["__I386__"] = "1";
    } else if (target == "m16c") {
        predefinedMacros["__M16C__"] = "1";
    } else if (target == "pic16") {
        predefinedMacros["__PIC16__"] = "1";
    } else if (target == "pic18") {
        predefinedMacros["__PIC18__"] = "1";
    } else if (target == "r10000") {
        predefinedMacros["__R10000__"] = "1";
    } else if (target == "r4000") {
        predefinedMacros["__R10000__"] = "1";
        predefinedMacros["__R4000__"] = "1";
    } else if (target == "cortex-m3") {
        predefinedMacros["__CORTEX_M3__"] = "1";
    } else if (target == "cortex-m4" || target == "cortex-m7") {
        predefinedMacros["__CORTEX_M3__"] = "1";
        predefinedMacros[target == "cortex-m4" ? "__CORTEX_M4__" : "__CORTEX_M7__"] = "1";
    } else if (target == "8051" || target == "8052" || target == "8031" || target == "8032") {
        predefinedMacros["__MCS51__"] = "1";
        if (target == "8051") predefinedMacros["__8051__"] = "1";
        else if (target == "8052") predefinedMacros["__8052__"] = "1";
        else if (target == "8031") predefinedMacros["__8031__"] = "1";
        else predefinedMacros["__8032__"] = "1";
    } else if (target == "armv7a") {
        predefinedMacros["__ARMV7A__"] = "1";
    } else if (target == "armv8a") {
        predefinedMacros["__ARMV8A__"] = "1";
        predefinedMacros["__aarch64__"] = "1";
    } else if (target == "powerpc") {
        predefinedMacros["__POWERPC__"] = "1";
        predefinedMacros["__ppc__"] = "1";
    } else if (target == "sparc") {
        predefinedMacros["__SPARC__"] = "1";
        predefinedMacros["__sparc__"] = "1";
    }

    // An external reference is allowed under a real object format for this
    // backend, and a hard compile error under any other format.
    bool allowExternal = backend->isObjectFormat(format);

    if (allowExternal && srcPaths.size() > 1) {
        // Each file needs its own fresh Backend instance, not the shared
        // `backend` above: generate() accumulates output in member
        // variables never reset between calls, so reusing one instance
        // would leak each file's generated text into the next one's object.
        try {
            for (const std::string &srcPath : srcPaths) {
                std::unique_ptr<pcc::Backend> fileBackend = makeBackend(target);
                if (hosted) {
                    auto *i386 = dynamic_cast<pcc::I386Codegen *>(fileBackend.get());
                    if (i386) {
                        i386->setHostedAbi(true);
                        i386->setWindowsAbi(windowsHosted);
                    }
                    if (auto *arm64 = dynamic_cast<pcc::Arm64Codegen *>(fileBackend.get())) {
                        arm64->setHostedAbi(macosHosted);
                    }
                }
                std::string asmText =
                    compileOneFile(srcPath, predefinedMacros, *fileBackend, allowExternal, optimize, dumpIr, dumpAsm);
                std::string bytes = assembleOne(*fileBackend, asmText, format, assemblerPath);
                writeToBuildDir(baseName(srcPath) + ".o", bytes, /*binary=*/true);
            }
        } catch (const std::exception &e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    // Exactly one input file, any format, defaulting to build/<name>.o
    // when --out is omitted for an object format.
    try {
        std::string asmText =
            compileOneFile(srcPaths[0], predefinedMacros, *backend, allowExternal, optimize, dumpIr, dumpAsm);
        std::string bytes = assembleOne(*backend, asmText, format, assemblerPath);
        if (backend->isObjectFormat(format) && outPath.empty()) {
            writeToBuildDir(baseName(srcPaths[0]) + ".o", bytes, /*binary=*/true);
        } else {
            writeResult(outPath, bytes);
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
