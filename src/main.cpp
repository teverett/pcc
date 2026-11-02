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
#include "optimizers/ConstantFolding.h"
#include "optimizers/DeadCodeElimination.h"
#include "preprocessor/Preprocessor.h"
#include "codegen/pdp7/Pdp7Codegen.h"
#include "codegen/x86_8086/I8086Codegen.h"
#include "coff/CoffReader.h"
#include "linker/I8086Linker.h"
#include "linker/Linker.h"
#include "linker/Pdp7Linker.h"
#include "linker/format/A7outWriter.h"
#include "linker/format/AoutWriter.h"
#include "linker/format/BinWriter.h"
#include "linker/format/CoffLinkWriter.h"
#include "linker/format/FormatWriter.h"
#include "linker/format/ListWriter.h"
#include "linker/format/PtrWriter.h"
#include "linker/format/RimWriter.h"

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

std::unique_ptr<pcc::Backend> makeBackend(const std::string &target) {
    if (target == "pdp7") return std::make_unique<pcc::Pdp7Codegen>();
    if (target == "8086") return std::make_unique<pcc::I8086Codegen>();
    return nullptr;
}

// The linker's own equivalent of makeBackend() above - one
// TargetLinker implementation per target, see linker/Linker.h.
std::unique_ptr<pcc::TargetLinker> makeLinker(const std::string &target) {
    if (target == "pdp7") return std::make_unique<pcc::Pdp7Linker>();
    if (target == "8086") return std::make_unique<pcc::I8086Linker>();
    return nullptr;
}

// One FormatWriter implementation per `--link` output format (see
// linker/FormatWriter.h) - decoupled from which TargetLinker
// produced the LinkedImage being written; a given `--target`'s own
// TargetLinker::outputFormats() lists which of these names it
// actually offers today.
std::unique_ptr<pcc::FormatWriter> makeFormatWriter(const std::string &format) {
    if (format == "coff") return std::make_unique<pcc::CoffLinkWriter>();
    if (format == "rim") return std::make_unique<pcc::RimWriter>();
    if (format == "ptr") return std::make_unique<pcc::PtrWriter>();
    if (format == "a7out") return std::make_unique<pcc::A7outWriter>();
    if (format == "list") return std::make_unique<pcc::ListWriter>();
    if (format == "bin") return std::make_unique<pcc::BinWriter>();
    if (format == "aout") return std::make_unique<pcc::AoutWriter>();
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

    pcc::ir::Program irProg = pcc::buildIR(prog, allowExternal);

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
        pcc::DeadCodeElimination dce;
        dce.run(irProg);
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
    // frontend, no Backend. Merging them into a resolved image (this
    // target's own TargetLinker - see linker/Linker.h) and writing
    // that image out in a given format (a separate FormatWriter -
    // see linker/FormatWriter.h) are two decoupled steps; `target`
    // only picks the first one, `format` the second. ---
    if (link) {
        std::unique_ptr<pcc::TargetLinker> linker = makeLinker(target);
        if (!linker) {
            std::cerr << "unknown --target '" << target << "' (known targets: pdp7, 8086)\n";
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
        std::unique_ptr<pcc::FormatWriter> writer = makeFormatWriter(format);
        // formatOk above already confirmed `format` is one of
        // `linker->outputFormats()`'s own names, and every one of
        // those always names a real FormatWriter (see
        // makeFormatWriter) - this can't actually be null.
        try {
            std::vector<pcc::CoffObject> objects;
            for (const std::string &path : srcPaths) {
                std::ifstream in(path, std::ios::binary);
                if (!in) throw std::runtime_error("could not read object file '" + path + "'");
                std::ostringstream ss;
                ss << in.rdbuf();
                objects.push_back(pcc::readCoffObject(ss.str()));
            }
            pcc::LinkedImage image = linker->merge(objects);
            writeResult(outPath, writer->write(image));
        } catch (const std::exception &e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    // --- ordinary compile mode. ---
    std::unique_ptr<pcc::Backend> backend = makeBackend(target);
    if (!backend) {
        std::cerr << "unknown --target '" << target << "' (known targets: pdp7, 8086)\n";
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
    if (format != "coff" && srcPaths.size() != 1) {
        std::cerr << "--format=" << format << " takes exactly one .c file (got " << srcPaths.size()
                   << ") - compile each file with --format=coff, then --link them together; see --help\n";
        return 1;
    }
    if (format == "coff" && srcPaths.size() > 1 && !outPath.empty()) {
        std::cerr << "--out can't be used with --format=coff and more than one input file - each file gets its "
                      "own build/<name>.o\n";
        return 1;
    }

    // Target macro pcc predefines for every file's own preprocessor
    // run - see preprocessor/Preprocessor.h and
    // docs/pdp-c-compiler.md's "Preprocessor" for `#ifdef __PDP7__`/
    // `#ifdef __8086__`-style target-conditional code.
    std::map<std::string, std::string> predefinedMacros;
    predefinedMacros[target == "pdp7" ? "__PDP7__" : "__8086__"] = "1";

    // A genuinely external reference is always allowed under
    // --format=coff (deferred as a relocation for a future --link
    // step) and always a hard compile error under any other format -
    // see IRBuilder.h's `allowExternal`.
    bool allowExternal = (format == "coff");

    if (format == "coff" && srcPaths.size() > 1) {
        // Multi-file --format=coff: each file compiles completely
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
        if (format == "coff" && outPath.empty()) {
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
