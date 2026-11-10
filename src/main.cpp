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
#include "linker/HostedLinker.h"
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
              << " [--target=<target>[-<host>]] [--format=asm|<target-specific>] [--out file] [--assembler path]\n"
                 "       [--optimize] [--dump-ir] [--dump-asm] file.c [file2.c ...]\n"
                 "   or: " << argv0
              << " --link [--target=<target>[-<host>]] --format=<target-native> [--gc-sections] [--out file] file1.o [file2.o ...]\n"
                 "\n"
                 "<target> is a backend name (e.g. pdp7, i386, armv8a) and <host> is the real OS\n"
                 "the produced executable runs on - 'none' (the default, meaning freestanding/\n"
                 "bare metal), 'linux', 'windows', or 'macos', wherever that target has one; see\n"
                 "--list-triples for every target-host combination actually supported. A plain\n"
                 "--target=<target> is shorthand for '-none'.\n"
                 "\n"
                 "A non-coff --format takes exactly one .c file - real separate compilation\n"
                 "is now the only way to combine more than one: compile each file on its own\n"
                 "with --format=coff (one invocation, several files, each compiled completely\n"
                 "independently - one build/<name>.o per file), then combine every .o with\n"
                 "--link into the final program. Hosted ELF/i386, PE/i386 and Mach-O/arm64\n"
                 "executables are linked internally. Standard object archives are selected lazily;\n"
                 "the built-in hosted C runtime provides puts; i386 ELF shared libraries and\n"
                 "function-only COFF import libraries are supported. External linker options,\n"
                 "linker scripts, and unsupported imports are rejected explicitly.\n";
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

// Every name makeBackend()/makeLinker() accept as --target (aliases sharing
// a backend, like 8088->8086, are listed separately since each is
// independently valid on the command line). Used both for "unknown
// --target" error messages and for --list-triples.
const std::vector<std::string> kTargetNames = {
    "pdp7", "8086", "8088", "6502", "65c02", "68000", "mc68328", "pdp11", "8080", "z80", "8008",
    "stm8", "stm32", "i386", "m16c", "pic16", "pic18", "r10000", "r4000", "cortex-m3", "cortex-m4",
    "cortex-m7", "8051", "8052", "8031", "8032", "armv7a", "armv8a", "powerpc", "sparc",
};

std::string knownTargetsList() {
    std::string result;
    for (size_t i = 0; i < kTargetNames.size(); ++i) result += (i ? ", " : "") + kTargetNames[i];
    return result;
}

// The host component of a <target>-<host> triple: which real OS (if any) a
// target's backend can produce a hosted executable for, via
// linker/HostedLinker.h (i386) or linker/NativeMacOSLinker.h (armv8a).
// "none" (freestanding/bare metal) is the only host every target supports.
std::vector<std::string> hostsFor(const std::string &target) {
    if (target == "i386") return {"none", "linux", "windows"};
    if (target == "armv8a") return {"none", "macos"};
    return {"none"};
}

// Parses a --target value shaped like a GNU-style <target>-<host> triple:
// pcc's own backend name is already the "target" component (the ISA pcc
// generates code for); "host" here is the real OS the produced executable
// will run on. A target name containing its own dash (e.g. cortex-m3) is
// unaffected, since the suffix after the last dash is only treated as a
// host when it actually names one.
struct ParsedTarget {
    std::string target;
    std::string host = "none";
};
ParsedTarget parseTargetTriple(const std::string &value) {
    static const std::vector<std::string> hostNames = {"none", "linux", "windows", "macos"};
    size_t dash = value.find_last_of('-');
    if (dash != std::string::npos) {
        std::string suffix = value.substr(dash + 1);
        if (std::find(hostNames.begin(), hostNames.end(), suffix) != hostNames.end()) {
            return {value.substr(0, dash), suffix};
        }
    }
    return {value, "none"};
}

void listTriples() {
    std::vector<std::string> triples;
    for (const auto &name : kTargetNames) {
        for (const auto &host : hostsFor(name)) triples.push_back(name + "-" + host);
    }
    std::sort(triples.begin(), triples.end());
    for (const auto &triple : triples) std::cout << triple << "\n";
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
    bool formatSet = false;
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
    bool gcSections = false;

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
            formatSet = true;
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
        } else if (arg == "--list-triples") {
            listTriples();
            return 0;
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
        } else if (arg == "--gc-sections") {
            gcSections = true;
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

    // Normalizes --target=<target>-<host> into plain `target` plus the
    // hosted/windowsHosted/macosHosted bools the rest of this function
    // branches on, and derives --format from the host when the caller
    // didn't give one explicitly (each non-"none" host has exactly one
    // format).
    {
        ParsedTarget parsed = parseTargetTriple(target);
        target = parsed.target;
        hosted = parsed.host != "none";
        windowsHosted = parsed.host == "windows";
        macosHosted = parsed.host == "macos";
        std::vector<std::string> validHosts = hostsFor(target);
        if (std::find(validHosts.begin(), validHosts.end(), parsed.host) == validHosts.end()) {
            std::cerr << "'" << target << "-" << parsed.host << "' is not a supported triple; " << target
                       << " supports:";
            for (const auto &host : validHosts) std::cerr << " " << target << "-" << host;
            std::cerr << "\n";
            return 1;
        }
        if (hosted) {
            std::string derivedFormat = parsed.host == "linux" ? "elf" : parsed.host == "windows" ? "coff" : "macho";
            if (formatSet && format != derivedFormat) {
                std::cerr << "--format=" << format << " doesn't match '" << target << "-" << parsed.host
                           << "' (expected --format=" << derivedFormat << ")\n";
                return 1;
            }
            format = derivedFormat;
        }
    }

    if (windowsLinkerKind != "msvc" && windowsLinkerKind != "mingw") {
        std::cerr << "--windows-linker must be 'msvc' or 'mingw'\n";
        return 1;
    }
    if (windowsLinkerKindSet && !windowsHosted) {
        std::cerr << "--windows-linker is only valid with --target=i386-windows\n";
        return 1;
    }
    if (windowsLinkerKindSet && !link) {
        std::cerr << "--windows-linker requires --link\n";
        return 1;
    }
    if (!link && (hosted || systemLinkerSet || librarySearchOptions)) {
        if (librarySearchOptions) {
            std::cerr << "-L/-l are only valid with --link and a hosted --target (e.g. --target=i386-linux)\n";
            return 1;
        }
        if (systemLinkerSet) {
            std::cerr << "--linker is only valid with --link and a hosted --target (e.g. --target=i386-linux)\n";
            return 1;
        }
    }
    if (link && (librarySearchOptions || systemLinkerSet) && !hosted) {
        std::cerr << "-L/-l/--linker require --link and a hosted --target (e.g. --target=i386-linux)\n";
        return 1;
    }
    if (gcSections && !link) {
        std::cerr << "--gc-sections requires --link\n";
        return 1;
    }
    if (gcSections && hosted) {
        std::cerr << "--gc-sections is only valid with --link and a non-hosted --target (e.g. --target=pdp7)\n";
        return 1;
    }

    // --link mode: merging .o files (TargetLinker) and writing the result
    // in a given format (FileFormatWriter) are decoupled; `target` picks
    // the first, `format` the second.
    if (link) {
        if (hosted) {
            if (srcPaths.empty()) {
                std::cerr << "--link with a hosted --target requires at least one object or library input\n";
                return 1;
            }
            try {
                std::string output = outPath.empty() ? "a.out" : outPath;
                if (systemLinkerSet || windowsLinkerKindSet) {
                    throw std::runtime_error("--linker and --windows-linker are unsupported: hosted linking is internal");
                }
                pcc::HostedTarget hostedTarget = windowsHosted ? pcc::HostedTarget::WindowsI386 :
                                                macosHosted ? pcc::HostedTarget::MacOSArm64 :
                                                              pcc::HostedTarget::LinuxI386;
                pcc::linkHosted(systemLinkInputs, hostedTarget, output);
            } catch (const std::exception &e) {
                std::cerr << e.what() << "\n";
                return 1;
            }
            return 0;
        }
        std::unique_ptr<pcc::TargetLinker> linker = makeLinker(target);
        if (!linker) {
            std::cerr << "unknown --target '" << target << "' (known targets: " << knownTargetsList() << ")\n";
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
            pcc::ObjectFile linked = linker->merge(objects, gcSections);
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
        std::cerr << "unknown --target '" << target << "' (known targets: " << knownTargetsList() << ")\n";
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
