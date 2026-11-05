// pcc - a compiler for pdp-c-compiler's C subset.
//
// Reads one or more C source files, each parsed on its own but then
// merged into a single program sharing one flat global/function
// namespace (see main()'s comment on mergeInto) before anything
// else happens - so a function or global defined in one file is
// visible to every other file given on the same command line, no
// forward-declaration or "extern" needed. Compiles the result to
// the chosen target's assembly text, via a Backend (see Codegen.h) -
// and, unless --format=asm was requested, pipes that through the
// target's own real assembler to produce a real binary. See
// README.md for the language subset (the same for every target) and
// each backend's own header comment for target-specific codegen
// conventions.

#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "codegen/Codegen.h"
#include "ast/BuildAst.h"
#include "CLexer.h"
#include "CParser.h"
#include "CommonTokenStream.h"
#include "ir/IRBuilder.h"
#include "ir/IRPrinter.h"
#include "ir/IRValidator.h"
#include "optimizers/algebraic_simplification/AlgebraicSimplification.h"
#include "optimizers/constant_folding/ConstantFolding.h"
#include "optimizers/dead_code_elimination/DeadCodeElimination.h"
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
              << " [--target=pdp7|8086] [--format=asm|<target-specific>] [--out file] [--assembler path]\n"
                 "       [--optimize] [--dump-ir] [--dump-asm] file.c [file2.c ...]\n"
                 "   or: " << argv0
              << " --link [--target=pdp7|8086] --format=<target-native> [--out file] file1.o [file2.o ...]\n"
                 "\n"
                 "A non-coff --format takes exactly one .c file - real separate compilation\n"
                 "is now the only way to combine more than one: compile each file on its own\n"
                 "with --format=coff (one invocation, several files, each compiled completely\n"
                 "independently - one build/<name>.o per file), then combine every .o with\n"
                 "--link into the final program.\n";
}

// The source file's name with its directory and ".c" stripped, e.g.
// "../src-c/sys/s5.c" -> "s5" - used to name --dump-ir/--dump-asm's
// output files.
std::string baseName(const std::string &srcPath) {
    size_t slash = srcPath.find_last_of('/');
    std::string base = (slash == std::string::npos) ? srcPath : srcPath.substr(slash + 1);
    if (base.size() > 2 && base.substr(base.size() - 2) == ".c") base = base.substr(0, base.size() - 2);
    return base;
}

// Writes `content` to build/<name>, creating build/ first if it
// doesn't exist yet (mirroring the Makefile's own BUILD_DIR). Used
// by --dump-ir/--dump-asm, which exist purely to let you inspect
// what pcc produced partway through compiling - not part of the
// normal compile-and-maybe-assemble pipeline.
// `binary`: --dump-ir/--dump-asm's own callers want text mode (the
// default); writing a `build/<name>.o` from multi-file --format=coff
// (see main()'s compileAndAssembleOne) wants binary, since a COFF
// object is arbitrary bytes, not text.
// mkdir()'s own signature differs by platform (MinGW's <io.h> one -
// the only one available there, since it has no <sys/stat.h> mkdir -
// takes no mode argument at all), so every caller goes through this
// instead of calling mkdir() directly.
void makeDir(const std::string &path) {
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}

void writeToBuildDir(const std::string &name, const std::string &content, bool binary = false) {
    makeDir("build");  // ignore EEXIST and any other failure here; the ofstream below will fail loudly instead
    std::ofstream out("build/" + name, binary ? std::ios::binary : std::ios::out);
    if (!out) {
        std::cerr << "warning: could not write build/" << name << "\n";
        return;
    }
    out << content;
}

// --target=8088 is a deliberate, software-identical alias for
// --target=8086, not a separate backend: the 8088 is the exact same
// instruction set as the 8086 (literally the same opcodes, the same
// registers, the same real-mode memory model) - its one real
// difference, an 8-bit external data bus instead of a 16-bit one, is
// a hardware bus-timing detail invisible to every instruction this
// compiler (or anything else) could ever emit. There is nothing for
// a distinct I8088Codegen to do differently, so this just reuses
// I8086Codegen/makeI8086Linker() outright rather than inventing a
// duplicate pair of classes with no actual behavioral difference.
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
    // --target=r4000 is a deliberate, software-identical alias for
    // --target=r10000 (same reasoning as the 8088 alias above): this
    // backend never emits anything beyond the real MIPS I
    // instruction subset (see MipsCodegen.h's own header comment),
    // which a real R4000 (MIPS III) runs exactly as unchanged as a
    // real R10000 (MIPS IV) does - there is nothing for a distinct
    // R4000 codegen to do differently.
    if (target == "r10000" || target == "r4000") return std::make_unique<pcc::MipsCodegen>();
    // --target=cortex-m4/cortex-m7 are deliberate, software-identical
    // aliases for --target=cortex-m3 (same reasoning as r4000/r10000
    // above): this backend only ever emits real ARMv7-M mainline
    // Thumb/Thumb-2 instructions (see ArmMCodegen.h's own header
    // comment), which the M4/M7 execute exactly the same as the M3 -
    // there is nothing for a distinct M4/M7 codegen to do differently
    // (this backend never touches either chip's own DSP/FP extension
    // instructions at all).
    if (target == "cortex-m3" || target == "cortex-m4" || target == "cortex-m7") {
        return std::make_unique<pcc::ArmMCodegen>();
    }
    // 8031/8032/8051/8052 are all the same MCS-51 instruction set as
    // far as any instruction this compiler could ever emit is
    // concerned - see MCS51Codegen.h's own header comment (same
    // "software-identical part numbers share one Backend" reasoning
    // as the 8088 alias just above).
    if (target == "8051" || target == "8052" || target == "8031" || target == "8032") {
        return std::make_unique<pcc::MCS51Codegen>();
    }
    // ARM A-profile: armv7a is 32-bit AArch32 (A32 encoding), armv8a
    // is 64-bit AArch64 (A64 encoding) - genuinely different
    // instruction sets (unlike every alias above), so each gets its
    // own Codegen rather than sharing one.
    if (target == "armv7a") return std::make_unique<pcc::ArmCodegen>();
    if (target == "armv8a") return std::make_unique<pcc::Arm64Codegen>();
    if (target == "powerpc") return std::make_unique<pcc::PowerPCCodegen>();
    if (target == "sparc") return std::make_unique<pcc::SparcCodegen>();
    return nullptr;
}

// The linker's own equivalent of makeBackend() above - one
// TargetLinker implementation per target, see linker/Linker.h.
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


// Parses one file's own source into its own Program (BuildAst
// doesn't know or care that there might be others), then appends its
// functions/globals onto `combined`. "Merging" is this simple
// because name resolution (IRBuilder) only happens once, after every
// file's declarations are already in one list - a function or
// global is visible to IRBuilder regardless of which file physically
// defines it, with no linking step beyond this concatenation. A
// prototype (`int f();`, no body) contributes nothing on its own -
// BuildAst already drops those - so one file can call a function
// defined in another without declaring it first at all; a prototype
// is purely documentation here, never required.
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

// Compiles exactly one source file completely independently - no
// visibility into any other file, even one given on the same pcc
// invocation (see IRBuilder.h's own `allowExternal`) - to this
// backend's own generated assembly text. `allowExternal` is real
// separate compilation's own switch (true only for --format=coff -
// see IRBuilder.h), threaded straight through to both buildIR() and
// Backend::generate()'s own `objectMode`.
std::string compileOneFile(const std::string &srcPath, const std::map<std::string, std::string> &predefinedMacros,
                             pcc::Backend &backend, bool allowExternal, bool optimize, bool dumpIr, bool dumpAsm) {
    pcc::Program prog;
    parseAndMergeFile(srcPath, pcc::preprocess(srcPath, predefinedMacros), prog);

    pcc::ir::Program irProg = pcc::buildIR(prog, backend.typeWidths(), allowExternal);

    // Every registered Optimizer (see optimizers/Optimizer.h) runs
    // here, before any backend ever sees the IR - target-independent
    // by construction, since instruction selection hasn't happened
    // yet.
    if (optimize) {
        // Constant folding first - it often turns a once-variable
        // initializer/condition into a literal DeadCodeElimination
        // can then reason about (e.g. a now-dead LocalDecl's own
        // initializer collapsing to a side-effect-free constant).
        pcc::ConstantFolding cf;
        cf.run(irProg);
        // Strength reduction next - it only ever rewrites an
        // operation whose OTHER operand isn't a literal (constant
        // folding already handled both-literal above), so running it
        // after constant folding rather than before doesn't lose any
        // opportunity either pass could find alone.
        pcc::AlgebraicSimplification algsimp;
        algsimp.run(irProg);
        // Local CSE next, so it sees each pass above's own
        // canonicalized form (e.g. a strength-reduced `x << 1` now
        // matches another occurrence written as `x << 1` to begin
        // with, where the pre-simplification `x * 2` wouldn't have) -
        // see LocalCSE.h's own "syntactic, not flow-sensitive" scope.
        pcc::LocalCSE cse;
        cse.run(irProg);
        pcc::DeadCodeElimination dce;
        dce.run(irProg);
        // LocalCSE may have introduced brand new locals (temps) after
        // buildIR() already assigned every *other* local's own frame
        // offset once - see recomputeFrameLayout's own comment.
        pcc::recomputeFrameLayout(irProg, backend.typeWidths());
        // Unused-global removal last, and only when this compile sees
        // the entire program (never under real separate compilation,
        // --format=coff's own per-file `allowExternal`) - see
        // UnusedGlobalElimination.h's own comment on why that matters,
        // and why it runs after DeadCodeElimination (a global touched
        // only from now-unreachable code must look unused too, not
        // "used" by code that no longer exists in the tree).
        if (!allowExternal) {
            pcc::UnusedGlobalElimination uge;
            uge.run(irProg);
        }
    }

    // A self-check, not a user-facing feature: parses this program's
    // own printed IR (see IRPrinter.h) against grammar/PccIRLexer.g4
    // + grammar/PccIRParser.g4 on every compile, optimized or not, so
    // a bug in an optimizer (or in the printer itself) that produces
    // IR the grammar doesn't recognize is caught immediately instead
    // of silently miscompiling. See IRValidator.h's own comment.
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

// Assembles `asmText` (one backend's own generate() output, for one
// file) into `format` - "asm" just means "asmText itself, nothing
// more to do". Prefers this backend's own in-process assembler (see
// Codegen.h's assembleInProcess()); falls back to its real external
// assembler tool (via a temp input/output file pair) only for a
// backend that doesn't have one. Throws std::runtime_error on any
// failure, so every caller (single-file compile, or each file of a
// multi-file --format=coff compile) handles errors the same way.
std::string assembleOne(const pcc::Backend &backend, const std::string &asmText, const std::string &format,
                          const std::string &assemblerPath) {
    if (format == "asm") return asmText;

    if (std::optional<std::string> result = backend.assembleInProcess(asmText, format)) return *result;

    // mkstemp() (no suffix) is the only one of the two available on
    // MinGW - mkstemps() (glibc/BSD only) isn't - so the input file's
    // required extension (some real external assemblers key off it)
    // is added with a rename afterward instead of mkstemps()'s own
    // suffix argument. Neither hardcodes "/tmp" (not a real path on
    // native Windows) - std::filesystem::temp_directory_path() is
    // this platform's own actual temp directory on every target.
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

// Writes `content` either to `outPath` (binary) or, if empty, to
// stdout (binary) - the common "where does the final output go"
// decision every mode below (compile, multi-file coff, --link)
// shares once it has its own finished bytes in hand.
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
    std::vector<std::string> srcPaths;
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
        }
    }

    if (srcPaths.empty()) {
        usage(argv[0]);
        return 1;
    }

    // --- --link mode: combine already-compiled .o files - no
    // frontend, no Backend. Merging them into a resolved, linked
    // ObjectFile (this target's own TargetLinker - see
    // linker/Linker.h) and writing that out in a given format (a
    // separate FileFormatWriter - see fileformat/FileFormat.h) are
    // two decoupled steps; `target` only picks the first one,
    // `format` the second. ---
    if (link) {
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
        // formatOk above already confirmed `format` is one of
        // `linker->outputFormats()`'s own names, and every target's
        // own outputFormats() only ever lists a real FileFormatKind's
        // own name - this can't actually be nullopt.
        std::unique_ptr<pcc::FileFormatWriter> writer = pcc::makeFileFormatWriter(*outKind);
        try {
            std::vector<pcc::ObjectFile> objects;
            for (const std::string &path : srcPaths) {
                std::ifstream in(path, std::ios::binary);
                if (!in) throw std::runtime_error("could not read object file '" + path + "'");
                std::ostringstream ss;
                ss << in.rdbuf();
                std::string bytes = ss.str();
                // Each input .o can be in any supported object format
                // (see fileformat/FileFormat.h) - sniffed from its
                // own leading bytes, not assumed to be COFF, so a
                // link step can freely combine objects compiled with
                // different `--format` choices.
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

    // Real separate compilation is now the *only* way to combine more
    // than one .c file into a final program (see Codegen.h's
    // `objectMode`/IRBuilder.h's `allowExternal`) - any other format
    // only ever meant "merge every file into one shared namespace
    // before resolving a single name", which no longer exists.
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

    // Target macro pcc predefines for every file's own preprocessor
    // run - see preprocessor/Preprocessor.h and
    // docs/pdp-c-compiler.md's "Preprocessor" for `#ifdef __PDP7__`/
    // `#ifdef __8086__`/`#ifdef __6502__`-style target-conditional code.
    std::map<std::string, std::string> predefinedMacros;
    if (target == "pdp7") {
        predefinedMacros["__PDP7__"] = "1";
    } else if (target == "8086") {
        predefinedMacros["__8086__"] = "1";
    } else if (target == "8088") {
        // --target=8088 is a software-identical alias for --target=8086
        // (see makeBackend()'s own comment) - defining __8086__ too
        // means existing `#ifdef __8086__` source (e.g. tests/disk.c's
        // real BIOS disk I/O, equally valid on either machine) keeps
        // working unchanged under either target name. __8088__ exists
        // only for source that wants to say "I was compiled for an
        // 8088" specifically, even though nothing in pcc itself ever
        // treats that differently from __8086__.
        predefinedMacros["__8086__"] = "1";
        predefinedMacros["__8088__"] = "1";
    } else if (target == "6502") {
        predefinedMacros["__6502__"] = "1";
    } else if (target == "65c02") {
        // Same reasoning as __8086__/__8088__ above: a 65C02 is a
        // real superset of the 6502, so source gated on plain
        // `#ifdef __6502__` (nothing CMOS-specific) keeps working
        // unchanged under this target too; __65C02__ exists only for
        // source that wants to say "I need the real CMOS-only
        // instructions" specifically (none of which this compiler's
        // own backend ever lets through its `asm` escape and back
        // into a `--format=raw` that doesn't build, since
        // M65C02Assembler.cpp only ever recognizes this one extra
        // mnemonic - `stz`/`stzp` - beyond the plain 6502 dialect).
        predefinedMacros["__6502__"] = "1";
        predefinedMacros["__65C02__"] = "1";
    } else if (target == "68000") {
        predefinedMacros["__68000__"] = "1";
    } else if (target == "mc68328") {
        // Same reasoning as __6502__/__65C02__ above: the MC68328's
        // own CPU core executes a bit-for-bit identical instruction
        // set to a plain 68000 (see MC68328Codegen.h's own header
        // comment), so source gated on plain `#ifdef __68000__`
        // keeps working unchanged under this target too; __MC68328__
        // exists only for source that wants to say "I need this real
        // chip specifically" (e.g. to assume its own real reset-
        // vector-table entry convention, rather than the generic
        // `68000` target's own simulator-PC-forced one).
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
        // --target=r4000 is a software-identical alias for
        // --target=r10000 (see makeBackend()'s own comment) - same
        // __8086__/__8088__ reasoning: defining __R10000__ too means
        // existing `#ifdef __R10000__` source (nothing here is ever
        // actually R10000-specific, since this backend only emits
        // the real MIPS I instruction subset) keeps working
        // unchanged under either target name. __R4000__ exists only
        // for source that wants to say "I was compiled for an
        // R4000" specifically.
        predefinedMacros["__R10000__"] = "1";
        predefinedMacros["__R4000__"] = "1";
    } else if (target == "cortex-m3") {
        predefinedMacros["__CORTEX_M3__"] = "1";
    } else if (target == "cortex-m4" || target == "cortex-m7") {
        // --target=cortex-m4/cortex-m7 are software-identical aliases
        // for --target=cortex-m3 (see makeBackend()'s own comment) -
        // same __8086__/__8088__ reasoning: defining __CORTEX_M3__
        // too means existing `#ifdef __CORTEX_M3__` source (nothing
        // here is ever actually M3-specific, since this backend only
        // emits the real ARMv7-M mainline instruction subset) keeps
        // working unchanged under either target name. __CORTEX_M4__/
        // __CORTEX_M7__ exist only for source that wants to say "I
        // was compiled for this exact chip" specifically.
        predefinedMacros["__CORTEX_M3__"] = "1";
        predefinedMacros[target == "cortex-m4" ? "__CORTEX_M4__" : "__CORTEX_M7__"] = "1";
    } else if (target == "8051" || target == "8052" || target == "8031" || target == "8032") {
        // All four part numbers share the exact same backend (see
        // makeBackend()'s own comment) and so always define the
        // common `__MCS51__` macro; each part number also gets its
        // own exact macro (`__8051__`/etc.) for source that wants to
        // say "I need this specific part" even though nothing in
        // pcc itself ever treats one differently from another.
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

    // A genuinely external reference is always allowed under a real
    // object format for this backend (`backend->isObjectFormat()` -
    // see Codegen.h's own header comment on why that's asked per-
    // backend rather than hardcoded to "coff": deferred as a
    // relocation for a future --link step) and always a hard compile
    // error under any other format - see IRBuilder.h's
    // `allowExternal`.
    bool allowExternal = backend->isObjectFormat(format);

    if (allowExternal && srcPaths.size() > 1) {
        // Multi-file object compile: each file compiles completely
        // independently (compileOneFile has no idea any other file
        // on this command line exists) and gets its own build/<name>.o.
        // Each needs its own fresh Backend instance, not the shared
        // `backend` above (used only for the --format/--assembler
        // validation before this loop) - generate() accumulates its
        // output in member variables (code_/trailer_/literal and temp
        // counters) that are never reset between calls, so reusing
        // one instance across files would silently leak every earlier
        // file's own generated text into every later one's object -
        // exactly the "every file defines every function" corruption
        // a real separate-compilation object must never produce.
        try {
            for (const std::string &srcPath : srcPaths) {
                std::unique_ptr<pcc::Backend> fileBackend = makeBackend(target);
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

    // Exactly one input file - any format, including --format=coff
    // with --out (or the default build/<name>.o if --out wasn't
    // given, same convention --dump-ir/--dump-asm already use).
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
