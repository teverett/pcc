// rcc - the retargetable C compiler, for a small C subset.
//
// Reads one or more C source files, parsed separately but merged into one
// program sharing a flat global/function namespace, then compiles the
// result via a Backend (see Codegen.h) and, unless --format=asm, assembles
// it with that target's own in-process assembler.

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
#include "optimizers/address_hoisting/AddressHoisting.h"
#include "optimizers/algebraic_simplification/AlgebraicSimplification.h"
#include "optimizers/constant_folding/ConstantFolding.h"
#include "optimizers/constant_propagation/ConstantPropagation.h"
#include "optimizers/dead_code_elimination/DeadCodeElimination.h"
#include "optimizers/dead_store_elimination/DeadStoreElimination.h"
#include "optimizers/identical_code_folding/IdenticalCodeFolding.h"
#include "optimizers/local_cse/LocalCSE.h"
#include "optimizers/unused_global_elimination/UnusedGlobalElimination.h"
#include "preprocessor/Preprocessor.h"
#include "codegen/pdp1/Pdp1Codegen.h"
#include "codegen/pdp7/Pdp7Codegen.h"
#include "codegen/x86_8086/I8086Codegen.h"
#include "codegen/m6502/M6502Codegen.h"
#include "codegen/m65c02/M65C02Codegen.h"
#include "codegen/m68k/M68kCodegen.h"
#include "codegen/mc68328/MC68328Codegen.h"
#include "codegen/pdp11/Pdp11Codegen.h"
#include "codegen/pdp10/Pdp10Codegen.h"
#include "codegen/i8080/I8080Codegen.h"
#include "codegen/m6809/M6809Codegen.h"
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
#include "codegen/vax/VaxCodegen.h"
#include "codegen/avr/AVRCodegen.h"
#include "codegen/alpha/AlphaCodegen.h"
#include "codegen/saturn/SaturnCodegen.h"
#include "codegen/tms320c6713/Tms320C6713Codegen.h"
#include "fileformat/FileFormat.h"
#include "linker/Linker.h"
#include "linker/WindowsLinuxLinker.h"
#include "linker/factory/I8086LinkerFactory.h"
#include "linker/factory/Pdp7LinkerFactory.h"
#include "linker/factory/Z80LinkerFactory.h"
#include "linker/factory/ColecoLinkerFactory.h"
#include "linker/factory/M6809LinkerFactory.h"
#include "linker/factory/M6502LinkerFactory.h"
#include "linker/factory/M65C02LinkerFactory.h"
#include "linker/factory/I8080LinkerFactory.h"
#include "linker/factory/I8008LinkerFactory.h"
#include "linker/factory/STM8LinkerFactory.h"
#include "linker/factory/MC68328LinkerFactory.h"
#include "linker/factory/M68kLinkerFactory.h"
#include "linker/factory/Pdp11LinkerFactory.h"
#include "linker/factory/Pdp10LinkerFactory.h"
#include "linker/factory/M16cLinkerFactory.h"
#include "linker/factory/I386LinkerFactory.h"
#include "linker/factory/ArmLinkerFactory.h"
#include "linker/factory/ArmMLinkerFactory.h"
#include "linker/factory/Arm64LinkerFactory.h"
#include "linker/factory/STM32LinkerFactory.h"
#include "linker/factory/MipsLinkerFactory.h"
#include "linker/factory/PowerPCLinkerFactory.h"
#include "linker/factory/SparcLinkerFactory.h"
#include "linker/factory/VaxLinkerFactory.h"
#include "linker/factory/AVRLinkerFactory.h"
#include "linker/factory/AlphaLinkerFactory.h"
#include "linker/factory/SaturnLinkerFactory.h"
#include "linker/factory/Tms320C6713LinkerFactory.h"
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
              << " [--target=<target>[-<host>]] [--format=asm|<target-specific>] [--out file]\n"
                 "       [--optimize] [--dump-ir] [--dump-asm] file.c [file2.c ...]\n"
                 "   or: " << argv0
              << " --link [--target=<target>[-<host>]] --format=<target-native> [--gc-sections] [--base=<word-addr>|0x<hex>|$<hex>] [--out file] file1.o [file2.o ...]\n"
                 "\n"
                 "<target> is a backend name (e.g. pdp7, i386, armv8a) and <host> is the real OS\n"
                 "the produced executable runs on - 'none' (the default, meaning freestanding/\n"
                 "bare metal), 'linux', 'windows', or 'macos', wherever that target has one. A\n"
                 "plain --target=<target> is shorthand for '-none'.\n"
                 "\n"
                 "--platform=<name> is sugar for a --target/--format pair under one name (e.g.\n"
                 "--platform=6502-dos33). --list-platforms prints every --target-host combination and\n"
                 "every --platform name this build actually accepts, one per line.\n"
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
// --target" error messages and for --list-platforms.
const std::vector<std::string> kTargetNames = {
    "pdp1", "pdp7", "8086", "8088", "8086-dos", "6502", "65c02", "atari2600", "2a03", "68000", "mc68328", "pdp11", "pdp10", "8080", "z80", "z80-coleco", "6809", "8008",
    "stm8", "stm32", "i386", "m16c", "m30626fjpfp", "pic16", "pic18", "r10000", "r4000", "cortex-m3",
    "cortex-m4", "cortex-m7", "8051", "8052", "8031", "8032", "armv7a", "armv8a", "powerpc", "sparc",
    "vax", "avr", "alpha", "saturn", "tms320c6713",
};

std::string knownTargetsList() {
    std::string result;
    for (size_t i = 0; i < kTargetNames.size(); ++i) result += (i ? ", " : "") + kTargetNames[i];
    return result;
}

// The host component of a <target>-<host> triple: which real OS (if any) a
// target's backend can produce a hosted executable for, via
// linker/WindowsLinuxLinker.h (i386) or linker/MacOSLinker.h (armv8a).
// "none" (freestanding/bare metal) is the only host every target supports.
std::vector<std::string> hostsFor(const std::string &target) {
    if (target == "i386") return {"none", "linux", "windows"};
    if (target == "armv8a") return {"none", "macos"};
    return {"none"};
}

// Parses a --target value shaped like a GNU-style <target>-<host> triple:
// rcc's own backend name is already the "target" component (the ISA rcc
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

// Several --target names below are deliberate, software-identical aliases
// for another target rather than distinct backends (8088->8086, r4000->
// r10000, cortex-m4/m7->cortex-m3, 8031/8032/8051/8052 all share one
// backend): each pair's real hardware differences are invisible to any
// instruction this compiler emits, so there's nothing for a separate
// Codegen to do differently.
std::unique_ptr<rcc::Backend> makeBackend(const std::string &target) {
    if (target == "pdp1") return std::make_unique<rcc::Pdp1Codegen>();
    if (target == "pdp7") return std::make_unique<rcc::Pdp7Codegen>();
    if (target == "8086" || target == "8088") return std::make_unique<rcc::I8086Codegen>();
    if (target == "8086-dos") return std::make_unique<rcc::I8086Codegen>(/*dosAbi=*/true);
    if (target == "6502") return std::make_unique<rcc::M6502Codegen>();
    if (target == "atari2600") return std::make_unique<rcc::M6502Codegen>(rcc::M6502Codegen::Variant::Atari2600);
    if (target == "2a03") return std::make_unique<rcc::M6502Codegen>(rcc::M6502Codegen::Variant::Nes2A03);
    if (target == "65c02") return std::make_unique<rcc::M65C02Codegen>();
    if (target == "68000") return std::make_unique<rcc::M68kCodegen>();
    if (target == "mc68328") return std::make_unique<rcc::MC68328Codegen>();
    if (target == "pdp11") return std::make_unique<rcc::Pdp11Codegen>();
    if (target == "pdp10") return std::make_unique<rcc::Pdp10Codegen>();
    if (target == "8080") return std::make_unique<rcc::I8080Codegen>();
    if (target == "z80" || target == "z80-coleco") return std::make_unique<rcc::Z80Codegen>();
    if (target == "6809") return std::make_unique<rcc::M6809Codegen>();
    if (target == "8008") return std::make_unique<rcc::I8008Codegen>();
    if (target == "stm8") return std::make_unique<rcc::STM8Codegen>();
    if (target == "stm32") return std::make_unique<rcc::STM32Codegen>();
    if (target == "i386") return std::make_unique<rcc::I386Codegen>();
    if (target == "m16c" || target == "m30626fjpfp") return std::make_unique<rcc::M16cCodegen>();
    if (target == "pic16") return std::make_unique<rcc::PIC16Codegen>();
    if (target == "pic18") return std::make_unique<rcc::PIC18Codegen>();
    if (target == "r10000" || target == "r4000") return std::make_unique<rcc::MipsCodegen>();
    if (target == "cortex-m3" || target == "cortex-m4" || target == "cortex-m7") {
        return std::make_unique<rcc::ArmMCodegen>();
    }
    if (target == "8051" || target == "8052" || target == "8031" || target == "8032") {
        return std::make_unique<rcc::MCS51Codegen>();
    }
    // armv7a (32-bit AArch32) and armv8a (64-bit AArch64) are genuinely
    // different instruction sets, each with its own Codegen.
    if (target == "armv7a") return std::make_unique<rcc::ArmCodegen>();
    if (target == "armv8a") return std::make_unique<rcc::Arm64Codegen>();
    if (target == "powerpc") return std::make_unique<rcc::PowerPCCodegen>();
    if (target == "sparc") return std::make_unique<rcc::SparcCodegen>();
    if (target == "vax") return std::make_unique<rcc::VaxCodegen>();
    if (target == "avr") return std::make_unique<rcc::AVRCodegen>();
    if (target == "alpha") return std::make_unique<rcc::AlphaCodegen>();
    if (target == "saturn") return std::make_unique<rcc::SaturnCodegen>();
    if (target == "tms320c6713") return std::make_unique<rcc::Tms320C6713Codegen>();
    return nullptr;
}

std::unique_ptr<rcc::TargetLinker> makeLinker(const std::string &target,
                                               std::optional<uint32_t> baseAddressWords = std::nullopt) {
    if (target == "pdp7") {
        return baseAddressWords ? rcc::makePdp7Linker(*baseAddressWords) : rcc::makePdp7Linker();
    }
    if (target == "8086" || target == "8088") return rcc::makeI8086Linker();
    if (target == "8086-dos") return rcc::makeI8086DosLinker();
    if (target == "z80") return rcc::makeZ80Linker();
    if (target == "z80-coleco") return rcc::makeColecoLinker();
    if (target == "6809") return rcc::makeM6809Linker();
    if (target == "6502") {
        return baseAddressWords ? rcc::makeM6502Linker(*baseAddressWords) : rcc::makeM6502Linker();
    }
    if (target == "atari2600") {
        return baseAddressWords ? rcc::makeAtari2600Linker(*baseAddressWords) : rcc::makeAtari2600Linker();
    }
    if (target == "2a03") {
        return baseAddressWords ? rcc::makeNes2A03Linker(*baseAddressWords) : rcc::makeNes2A03Linker();
    }
    if (target == "65c02") {
        return baseAddressWords ? rcc::makeM65C02Linker(*baseAddressWords) : rcc::makeM65C02Linker();
    }
    if (target == "8080") return rcc::makeI8080Linker();
    if (target == "8008") return rcc::makeI8008Linker();
    if (target == "stm8") return rcc::makeSTM8Linker();
    if (target == "mc68328") return rcc::makeMC68328Linker();
    if (target == "68000") return rcc::makeM68kLinker();
    if (target == "pdp11") return rcc::makePdp11Linker();
    if (target == "pdp10") return rcc::makePdp10Linker();
    if (target == "m16c" || target == "m30626fjpfp") return rcc::makeM16cLinker();
    if (target == "i386") return rcc::makeI386Linker();
    if (target == "armv7a") return rcc::makeArmLinker();
    if (target == "armv8a") return rcc::makeArm64Linker();
    if (target == "cortex-m3" || target == "cortex-m4" || target == "cortex-m7") return rcc::makeArmMLinker();
    if (target == "stm32") return rcc::makeSTM32Linker();
    if (target == "r10000" || target == "r4000") return rcc::makeMipsLinker();
    if (target == "powerpc") return rcc::makePowerPCLinker();
    if (target == "sparc") return rcc::makeSparcLinker();
    if (target == "vax") return rcc::makeVaxLinker();
    if (target == "avr") return rcc::makeAVRLinker();
    if (target == "alpha") return rcc::makeAlphaLinker();
    if (target == "saturn") return rcc::makeSaturnLinker();
    if (target == "tms320c6713") return rcc::makeTms320C6713Linker();
    if (target == "8051" || target == "8052" || target == "8031" || target == "8032") return rcc::makeMCS51Linker();
    if (target == "pic16") return rcc::makePIC16Linker();
    if (target == "pic18") return rcc::makePIC18Linker();
    return nullptr;
}

// Every human-facing "<target>-<format>" platform name this project
// supports, e.g. "6502-dos33" or "z80-coleco-colecorom", built by
// enumerating kTargetNames x each target's own valid output formats -
// never by splitting a name string back apart, so a target name that
// itself contains a dash (z80-coleco, 8086-dos, cortex-m3) is never
// ambiguous. A target with a real TargetLinker uses its
// outputFormats() (the --link-stage formats); `pdp1` (no TargetLinker/
// --link step at all) and `tms320c6713` (outputFormats() is empty -
// asm-only) fall back to their own Backend::assembledFormats() instead,
// so every target still appears here even with no --link support - "asm"
// is dropped from that fallback since it's "print the assembly text",
// not a file-produced convention, consistent with how outputFormats()
// never lists it either. The three hosted triples (i386-linux/windows,
// armv8a-macos) already fully name their own single, implied format
// (see the `derivedFormat` logic below) and need no format suffix, so
// they're listed as plain target-host pairs with an empty `format` -
// --platform= resolution leaves `format` unset for these and lets that
// existing derivation run normally.
struct Platform {
    std::string name;
    std::string target;
    std::string format;  // empty for a hosted triple, whose format is implied, not chosen.
};
std::vector<Platform> allPlatforms() {
    std::vector<Platform> platforms;
    for (const auto &name : kTargetNames) {
        std::vector<std::string> formats;
        std::unique_ptr<rcc::TargetLinker> linker = makeLinker(name);
        if (linker) formats = linker->outputFormats();
        // tms320c6713's own linker exists but has an empty outputFormats()
        // (asm-only, no relocatable object format at all - see that
        // target's own LinkerFactory header comment); pdp1 has no
        // TargetLinker/--link step at all. Either way, fall back to the
        // target's own single-file-compile Backend::assembledFormats() so
        // every target still appears in this table.
        if (formats.empty()) {
            std::unique_ptr<rcc::Backend> backend = makeBackend(name);
            for (const auto &f : backend->assembledFormats()) {
                if (f != "asm") formats.push_back(f);
            }
        }
        for (const auto &format : formats) platforms.push_back({name + "-" + format, name, format});
    }
    for (const auto &target : {std::string("i386"), std::string("armv8a")}) {
        for (const auto &host : hostsFor(target)) {
            if (host != "none") platforms.push_back({target + "-" + host, target + "-" + host, ""});
        }
    }
    std::sort(platforms.begin(), platforms.end(), [](const Platform &a, const Platform &b) { return a.name < b.name; });
    return platforms;
}

void listPlatforms() {
    for (const auto &platform : allPlatforms()) std::cout << platform.name << "\n";
}

// Parses one file into its own Program, then appends its functions/globals
// onto `combined`; name resolution (IRBuilder) runs once, afterward, over
// the whole concatenated list, so no file needs to declare another's
// function first.
void parseAndMergeFile(const std::string &srcPath, const std::string &source, rcc::Program &combined) {
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

    rcc::Program fileProg = rcc::buildProgram(tree);
    for (auto &fn : fileProg.functions) {
        fn.sourceFile = srcPath;
        combined.functions.push_back(std::move(fn));
    }
    for (auto &g : fileProg.globals) {
        g.sourceFile = srcPath;
        combined.globals.push_back(std::move(g));
    }
    for (auto &s : fileProg.structs) {
        s.sourceFile = srcPath;
        combined.structs.push_back(std::move(s));
    }
}

// Compiles exactly one source file with no visibility into any other file
// on this invocation. `allowExternal` (true only for --format=coff) is
// threaded through to both buildIR() and Backend::generate()'s objectMode.
std::string compileOneFile(const std::string &srcPath, const std::map<std::string, std::string> &predefinedMacros,
                             rcc::Backend &backend, bool allowExternal, bool optimize, bool dumpIr, bool dumpAsm) {
    rcc::Program prog;
    parseAndMergeFile(srcPath, rcc::preprocess(srcPath, predefinedMacros), prog);

    rcc::ir::Program irProg = rcc::buildIR(prog, backend.typeWidths(), allowExternal);

    if (optimize) {
        // Constant propagation first: substituting a Var read with its
        // known literal value both feeds ConstantFolding new
        // literal-literal expressions to fold and, once a local's only
        // reads are gone, feeds DeadCodeElimination a now-unused
        // declaration to drop.
        rcc::ConstantPropagation cprop;
        cprop.run(irProg, srcPath);
        // Constant folding next, so DeadCodeElimination can reason about
        // initializers it collapses to a side-effect-free constant.
        rcc::ConstantFolding cf;
        cf.run(irProg, srcPath);
        // Strength reduction only rewrites an op whose other operand isn't
        // a literal, so ordering after constant folding loses nothing.
        rcc::AlgebraicSimplification algsimp;
        algsimp.run(irProg, srcPath);
        // Local CSE after the above so it matches each pass's canonicalized
        // form (e.g. a strength-reduced `x << 1`, not the original `x * 2`).
        rcc::LocalCSE cse;
        cse.run(irProg, srcPath);
        // After LocalCSE for the same canonicalization reason. Needs
        // backend.typeWidths() (to scale a hoisted index by the array's
        // element width), so unlike the passes above it isn't an
        // Optimizer - see AddressHoisting.h.
        rcc::AddressHoisting addrHoist;
        addrHoist.run(irProg, backend.typeWidths(), srcPath);
        // Before DeadCodeElimination, so a LocalDecl this pass strips down
        // to a bare, unread declaration looks unused to it too.
        rcc::DeadStoreElimination dse;
        dse.run(irProg, srcPath);
        rcc::DeadCodeElimination dce;
        dce.run(irProg, srcPath);
        // LocalCSE may have introduced new locals after buildIR() already
        // assigned frame offsets once.
        rcc::recomputeFrameLayout(irProg, backend.typeWidths());
        // Only when this compile sees the entire program: dropping a
        // function/global's own definition is only safe once every
        // reference to it is known, which --format=coff's separate
        // compilation doesn't guarantee.
        if (!allowExternal) {
            rcc::IdenticalCodeFolding icf;
            icf.run(irProg, srcPath);
            rcc::UnusedGlobalElimination uge;
            uge.run(irProg, srcPath);
        }
    }

    rcc::validateIRSyntax(irProg);

    if (dumpIr) writeToBuildDir(baseName(srcPath) + ".ir", rcc::printIR(irProg, srcPath));

    std::string asmText = backend.generate(irProg, /*objectMode=*/allowExternal);
    if (dumpAsm) {
        std::map<std::string, std::string> perFile = backend.perFileAsm();
        auto it = perFile.find(srcPath);
        if (it != perFile.end()) writeToBuildDir(baseName(srcPath) + backend.sourceExtension(), it->second);
    }
    return asmText;
}

// Assembles `asmText` into `format`; "asm" is a no-op. Every backend
// assembles every format it lists in assembledFormats() in-process (see
// Codegen.h's own assembleInProcess()) - there's no external assembler
// fallback.
std::string assembleOne(const rcc::Backend &backend, const std::string &asmText, const std::string &format) {
    if (format == "asm") return asmText;

    if (std::optional<std::string> result = backend.assembleInProcess(asmText, format)) return *result;

    throw std::runtime_error("internal: " + backend.name() + " backend has no in-process assembler for format '" +
                               format + "'");
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
    std::string systemLinker = "ld";
    bool systemLinkerSet = false;
    std::vector<std::string> srcPaths;
    std::vector<std::string> systemLinkInputs;
    bool targetSet = false;
    bool formatSet = false;
    std::string platformName;
    bool platformSet = false;
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
    std::optional<uint32_t> baseAddressWords;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto takeValue = [&](const std::string &flag) -> std::string {
            if (arg.size() > flag.size() && arg.compare(0, flag.size(), flag) == 0) return arg.substr(flag.size());
            return "";
        };
        std::string v;
        if (!(v = takeValue("--target=")).empty()) {
            target = v;
            targetSet = true;
        } else if (!(v = takeValue("--format=")).empty()) {
            format = v;
            formatSet = true;
        } else if (!(v = takeValue("--platform=")).empty()) {
            platformName = v;
            platformSet = true;
        } else if (!(v = takeValue("--out=")).empty()) {
            outPath = v;
        } else if (!(v = takeValue("--linker=")).empty()) {
            systemLinker = v;
            systemLinkerSet = true;
        } else if (!(v = takeValue("--windows-linker=")).empty()) {
            windowsLinkerKind = v;
            windowsLinkerKindSet = true;
        } else if (arg == "--list-platforms") {
            listPlatforms();
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
        } else if (!(v = takeValue("--base=")).empty()) {
            std::string numeric = v;
            int radix = 10;
            if (!numeric.empty() && numeric[0] == '$') {
                numeric.erase(0, 1);
                radix = 16;
            } else if (numeric.size() > 2 && numeric[0] == '0' &&
                       (numeric[1] == 'x' || numeric[1] == 'X')) {
                radix = 16;
            }
            try {
                size_t pos = 0;
                unsigned long parsed = std::stoul(numeric, &pos, radix);
                if (pos != numeric.size()) {
                    throw std::invalid_argument(numeric);
                }
                baseAddressWords = static_cast<uint32_t>(parsed);
            } catch (const std::exception &) {
                std::cerr << "--base must be a decimal or hex (0x../$..) word address, got '" << v << "'\n";
                return 1;
            }
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

    // --platform is pure sugar over --target/--format, resolved to exactly
    // those two before anything else below ever sees it - never a separate
    // code path past this point.
    if (platformSet) {
        if (targetSet || formatSet) {
            std::cerr << "--platform can't be combined with --target or --format - pick one\n";
            return 1;
        }
        std::vector<Platform> platforms = allPlatforms();
        auto it = std::find_if(platforms.begin(), platforms.end(),
                                [&](const Platform &p) { return p.name == platformName; });
        if (it == platforms.end()) {
            std::cerr << "unknown --platform '" << platformName << "' - see --list-platforms for every valid name\n";
            return 1;
        }
        target = it->target;
        if (!it->format.empty()) {
            // A platform name's format describes the FINAL product, which
            // only exists once every file is linked - it says nothing
            // about how to compile any one file along the way. So in
            // compile mode (no --link yet), the same --platform=<name>
            // used for the eventual --link step also works unchanged for
            // every earlier `--format=coff` compile step: assume `coff`
            // (the one real relocatable object format every target with
            // a TargetLinker can compile to) instead of the platform's
            // own final format, which is only honored once --link is
            // actually given. A target with no TargetLinker at all
            // (pdp1/tms320c6713 - see allPlatforms()) has no `coff` to
            // assume, so its own single format is used as-is either way.
            std::unique_ptr<rcc::Backend> platformBackend = makeBackend(target);
            std::vector<std::string> compileFormats = platformBackend->assembledFormats();
            bool hasCoff = std::find(compileFormats.begin(), compileFormats.end(), "coff") != compileFormats.end();
            format = (!link && hasCoff) ? "coff" : it->format;
            formatSet = true;
        }
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
    if (baseAddressWords && !link) {
        std::cerr << "--base requires --link\n";
        return 1;
    }
    if (baseAddressWords && target != "pdp7" && target != "6502" && target != "65c02" && target != "atari2600" &&
        target != "2a03") {
        std::cerr << "--base is only supported for --target=pdp7/6502/65c02/atari2600/2a03 right now\n";
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
                rcc::HostedTarget hostedTarget = windowsHosted ? rcc::HostedTarget::WindowsI386 :
                                                macosHosted ? rcc::HostedTarget::MacOSArm64 :
                                                              rcc::HostedTarget::LinuxI386;
                rcc::linkHosted(systemLinkInputs, hostedTarget, output);
            } catch (const std::exception &e) {
                std::cerr << e.what() << "\n";
                return 1;
            }
            return 0;
        }
        std::unique_ptr<rcc::TargetLinker> linker = makeLinker(target, baseAddressWords);
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
        auto outKind = rcc::fileFormatKindFromName(format);
        // formatOk above already confirmed format names a real FileFormatKind.
        std::unique_ptr<rcc::FileFormatWriter> writer = rcc::makeFileFormatWriter(*outKind);
        try {
            std::vector<rcc::ObjectFile> objects;
            for (const std::string &path : srcPaths) {
                std::ifstream in(path, std::ios::binary);
                if (!in) throw std::runtime_error("could not read object file '" + path + "'");
                std::ostringstream ss;
                ss << in.rdbuf();
                std::string bytes = ss.str();
                // Sniffed from leading bytes, not assumed to be COFF, so
                // objects compiled with different --format choices can
                // still be linked together.
                rcc::FileFormatKind kind = rcc::detectFileFormat(bytes);
                objects.push_back(rcc::makeFileFormatReader(kind)->read(bytes));
            }
            rcc::ObjectFile linked = linker->merge(objects, gcSections);
            writeResult(outPath, writer->write(linked));
        } catch (const std::exception &e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    // --- ordinary compile mode. ---
    std::unique_ptr<rcc::Backend> backend = makeBackend(target);
    if (!backend) {
        std::cerr << "unknown --target '" << target << "' (known targets: " << knownTargetsList() << ")\n";
        return 1;
    }
    if (hosted) {
        auto *i386 = dynamic_cast<rcc::I386Codegen *>(backend.get());
        if (i386) {
            i386->setHostedAbi(true);
            i386->setWindowsAbi(windowsHosted);
        }
        if (auto *arm64 = dynamic_cast<rcc::Arm64Codegen *>(backend.get())) arm64->setHostedAbi(macosHosted);
    }

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
    // mc68328, r4000, cortex-m4/m7, 8051/8052/8031/8032's siblings,
    // atari2600, 2a03) defines both its base target's macro and its own,
    // so source gated on either the generic or the specific name keeps
    // working.
    std::map<std::string, std::string> predefinedMacros;
    if (target == "pdp1") {
        predefinedMacros["__PDP1__"] = "1";
    } else if (target == "pdp7") {
        predefinedMacros["__PDP7__"] = "1";
    } else if (target == "8086") {
        predefinedMacros["__8086__"] = "1";
    } else if (target == "8088") {
        predefinedMacros["__8086__"] = "1";
        predefinedMacros["__8088__"] = "1";
    } else if (target == "8086-dos") {
        predefinedMacros["__8086__"] = "1";
        predefinedMacros["__MSDOS__"] = "1";
    } else if (target == "6502") {
        predefinedMacros["__6502__"] = "1";
    } else if (target == "atari2600") {
        predefinedMacros["__6502__"] = "1";
        predefinedMacros["__ATARI2600__"] = "1";
    } else if (target == "2a03") {
        predefinedMacros["__6502__"] = "1";
        predefinedMacros["__2A03__"] = "1";
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
    } else if (target == "pdp10") {
        predefinedMacros["__PDP10__"] = "1";
    } else if (target == "8080") {
        predefinedMacros["__8080__"] = "1";
    } else if (target == "z80") {
        predefinedMacros["__Z80__"] = "1";
    } else if (target == "z80-coleco") {
        predefinedMacros["__Z80__"] = "1";
        predefinedMacros["__COLECO__"] = "1";
    } else if (target == "6809") {
        predefinedMacros["__6809__"] = "1";
    } else if (target == "8008") {
        predefinedMacros["__8008__"] = "1";
    } else if (target == "stm8") {
        predefinedMacros["__STM8__"] = "1";
    } else if (target == "stm32") {
        predefinedMacros["__STM32__"] = "1";
    } else if (target == "i386") {
        predefinedMacros["__I386__"] = "1";
    } else if (target == "m16c" || target == "m30626fjpfp") {
        predefinedMacros["__M16C__"] = "1";
        if (target == "m30626fjpfp") predefinedMacros["__M30626FJPFP__"] = "1";
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
    } else if (target == "vax") {
        predefinedMacros["__VAX__"] = "1";
        predefinedMacros["__vax__"] = "1";
    } else if (target == "avr") {
        predefinedMacros["__AVR__"] = "1";
        predefinedMacros["__AVR_ATmega328P__"] = "1";
    } else if (target == "alpha") {
        predefinedMacros["__ALPHA__"] = "1";
        predefinedMacros["__alpha__"] = "1";
    } else if (target == "saturn") {
        predefinedMacros["__SATURN__"] = "1";
    } else if (target == "tms320c6713") {
        predefinedMacros["__TMS320C6713__"] = "1";
        predefinedMacros["__tms320c6713__"] = "1";
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
                std::unique_ptr<rcc::Backend> fileBackend = makeBackend(target);
                if (hosted) {
                    auto *i386 = dynamic_cast<rcc::I386Codegen *>(fileBackend.get());
                    if (i386) {
                        i386->setHostedAbi(true);
                        i386->setWindowsAbi(windowsHosted);
                    }
                    if (auto *arm64 = dynamic_cast<rcc::Arm64Codegen *>(fileBackend.get())) {
                        arm64->setHostedAbi(macosHosted);
                    }
                }
                std::string asmText =
                    compileOneFile(srcPath, predefinedMacros, *fileBackend, allowExternal, optimize, dumpIr, dumpAsm);
                std::string bytes = assembleOne(*fileBackend, asmText, format);
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
        std::string bytes = assembleOne(*backend, asmText, format);
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
