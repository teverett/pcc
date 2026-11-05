#include "I386Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "I386Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where this backend's own image is loaded and runs - the
// conventional Multiboot load address (comfortably above the BIOS/
// real-mode data area, with no need to avoid stepping on anything
// below 1MB the way a real-mode target has to).
constexpr uint32_t kLoadAddr = 0x100000;

// A real Multiboot header (see the Multiboot Specification's "OS
// image format") - the standard, real-bootloader-agnostic contract a
// 32-bit protected-mode kernel image uses to tell whatever loaded it
// (GRUB, or here, `qemu-system-i386 -kernel` acting as one - see
// tests/run_tests.sh's own check_i386()) where to load it and where
// to start running, *without* this backend ever having to write any
// code of its own to get from real mode into protected mode: the
// loader does that part entirely, before this image's first
// instruction ever runs - confirmed directly by booting exactly this
// header (reading `CS`/`EAX` over qemu's own monitor) rather than
// assumed from the spec alone. `flags` has only the "load this
// image's raw bytes at a fixed physical address" bit (0x10000, the
// "a.out kludge" - no symbol table to omit, no alignment/memory-map
// request needed for a program this simple); `header_addr`/
// `load_addr` are both `kLoadAddr` (the header is the very first
// thing in the image); `load_end_addr`/`bss_end_addr` of 0 both mean
// "load the whole file, no separate zero-filled bss"; `entry_addr`
// is `_start`, right after this 32-byte header.
constexpr uint32_t kMbMagic = 0x1BADB002;
constexpr uint32_t kMbFlags = 0x10000;
constexpr uint32_t kMbChecksum = static_cast<uint32_t>(-(static_cast<int64_t>(kMbMagic) + kMbFlags));
}  // namespace

// A literal element's own decimal text; a symbol element is the
// referenced Slot's or Function's own label, written out bare - "dd"
// (I386Assembler.cpp's Line::Dd) already evaluates any expr operand,
// symbol included, through the same emitAbs32 choke point a bare
// label used as a 32-bit immediate already goes through for
// genAddrOf()'s "mov eax,<label>" form, so a relocation is recorded
// for it exactly the same way.
std::string I386Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

void I386Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    int w = widthOfType(elemType);
    // No 64-bit data directive in this dialect - a `long long`
    // array's own literal elements are emitted as two "dd"s (low
    // dword, then high) per element instead; a symbol element always
    // means a plain native-int-width address (the dispatch-table
    // idiom - see AST.h's ArrayInitElem), so it still gets one "dd".
    const char *directive = w == 1 ? "db" : w == 2 ? "dw" : "dd";
    std::ostringstream line;
    line << lbl << ": " << directive << " ";
    bool first = true;
    for (const auto &e : values) {
        if (w == 8 && !e.isSymbol) {
            auto v = static_cast<uint64_t>(e.value);
            if (!first) line << ", ";
            line << static_cast<uint32_t>(v) << ", " << static_cast<uint32_t>(v >> 32);
            first = false;
            continue;
        }
        if (!first) line << ", ";
        line << arrayElemText(e);
        first = false;
    }
    trailer_.push_back(line.str());
}

std::string I386Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/`aout`
    // from a single file - see IRBuilder.h's `allowExternal`) means
    // `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits the Multiboot header
    // or entry sequence itself - I386LinkerFactory.cpp is solely
    // responsible for synthesizing exactly one, once every object is
    // merged and `main`'s linked address is resolvable (see
    // Linker.h). An object file's own base is `org 0` - it's the
    // linker's job to place it, not this backend's (see
    // I386Assembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kLoadAddr) << "\n\n";
    if (!objectMode) {
        code_ << "dd " << kMbMagic << "," << kMbFlags << "," << kMbChecksum << "\n";
        code_ << "dd " << kLoadAddr << "," << kLoadAddr << ",0,0\n";
        code_ << "dd _start\n\n";
        code_ << "_start:\n";
        // Masks both real 8259 PICs - confirmed directly that without
        // this, a program calling `_ion()` (this file's own `sti`, in
        // genExpr() below) triple-faults the moment a real hardware
        // timer tick arrives, since this backend never builds a real
        // IDT for one to vector through (same "real but nothing to
        // usefully do" stance as M68kCodegen.cpp's own `_ion()`/
        // `_iof()` comment - masking the PICs is what makes that
        // stance actually safe here, not just notionally true).
        code_ << "    mov al,0xFF\n    out 0x21,al\n";
        code_ << "    mov al,0xFF\n    out 0xA1,al\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The result sits in eax the moment execution reaches `halt` -
        // see I386Codegen.h's own header comment on why that's the
        // authoritative place to read it from (a real, directly
        // inspectable 32-bit register).
        code_ << "    hlt\n";
        code_ << "halt:\n    jmp halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - it must never get a local
        // storage word here, only ever be referenced by name (see
        // label()) in code, letting the assembler's own undefined-
        // symbol handling defer it to a relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w == 8) {
                auto uv = static_cast<uint64_t>(v);
                trailer_.push_back(lbl + ": dd " + std::to_string(static_cast<uint32_t>(uv)) + ", " +
                                    std::to_string(static_cast<uint32_t>(uv >> 32)) + "  ; " + slot->sourceName);
            } else {
                const char *directive = w == 1 ? "db" : w == 2 ? "dw" : "dd";
                trailer_.push_back(lbl + ": " + directive + " " + std::to_string(v) + "  ; " + slot->sourceName);
            }
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only - see M6502Codegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> I386Codegen::perFileAsm() const { return perFileAsm_; }

void I386Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    push ebp\n    mov ebp,esp\n";
    if (fn.func->frameSize > 0) code_ << "    sub esp," << fn.func->frameSize << "\n";

    genStmt(*fn.body);

    code_ << "    mov esp,ebp\n    pop ebp\n";
    code_ << "    ret\n\n";
}

void I386Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            if (isWide(s.cond->type)) code_ << "    or eax,edx\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cmp eax,0\n    je " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    jmp " << Ldone << "\n";
                code_ << Lelse << ":\n";
                genStmt(*s.elseStmt);
            }
            code_ << Ldone << ":\n";
            return;
        }
        case ir::StmtKind::While: {
            std::string Lstart = newLabel();
            std::string Lend = newLabel();
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            if (isWide(s.cond->type)) code_ << "    or eax,edx\n";
            code_ << "    cmp eax,0\n    je " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    jmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    jmp " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    jmp " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            code_ << "    mov esp,ebp\n    pop ebp\n";
            code_ << "    ret\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                code_ << "    mov " << addr(s.declSlot) << ",eax\n";
            }
            return;
        }
        case ir::StmtKind::ExprStmt: {
            genExpr(*s.initOrReturnOrExpr);
            return;
        }
        case ir::StmtKind::Empty:
            return;
        case ir::StmtKind::Asm: {
            code_ << s.asmText;
            if (!s.asmText.empty() && s.asmText.back() != '\n') code_ << "\n";
            return;
        }
    }
}

void I386Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                code_ << "    mov eax," << static_cast<uint32_t>(v) << "\n";
                code_ << "    mov edx," << static_cast<uint32_t>(v >> 32) << "\n";
                return;
            }
            code_ << "    mov eax," << e.intValue << "\n";
            return;
        }
        case ir::ExprKind::Var: {
            loadOperand(operandOf(e.slot), operandOf(e.slot, 4), e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                std::string addr = computeElementAddr(e.slot, *e.lhs);
                code_ << "    push ebx\n";
                genExpr(*e.rhs);
                code_ << "    pop ebx\n";
                storeOperand("[ebx]", "[ebx+4]", e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            storeOperand(operandOf(e.slot), operandOf(e.slot, 4), e.slot->type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Deref is untyped (plain ints with '*'/'&' applied, see
            // Type.h's own header comment) - always a full native-int
            // word, unaffected by the new type system.
            genExpr(*e.lhs);  // eax = pointer value
            code_ << "    mov ebx,eax\n    push ebx\n";
            genExpr(*e.rhs);  // eax = value to store
            code_ << "    pop ebx\n";
            code_ << "    mov [ebx],eax\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    genNegate64();
                } else {
                    code_ << "    neg eax\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // eax = pointer value
                code_ << "    mov ebx,eax\n    mov eax,[ebx]\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    not eax\n";
                if (isWide(e.rhs->type)) code_ << "    not edx\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Real cdecl-style argument passing - push right-to-left
            // so param 0 ends up closest to the return address (see
            // I8086Codegen.h's own `addr()` comment), caller cleans up
            // with `add esp,N` after the call returns. See
            // I8086Codegen.cpp's own identical `Call` case. Every
            // argument reserves at least one native int's worth (the
            // same "no narrower than a native word" rule
            // IRBuilder.cpp's own param-offset layout already applies
            // - there's no "push a single byte" on real i386), and a
            // wide (`long long`) argument pushes its high dword first
            // so its low dword ends up at the lower address, matching
            // a wide Param's own frameOffset/frameOffset+4 layout.
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    code_ << "    push edx\n    push eax\n";
                    totalBytes += 8;
                } else {
                    code_ << "    push eax\n";
                    totalBytes += 4;
                }
            }
            code_ << "    call " << label(e.func) << "\n";
            if (totalBytes > 0) code_ << "    add esp," << totalBytes << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> cell to write one into until the callee is
            // known, which it never is at compile time for this kind.
            // "call eax" (I386Assembler.cpp's new FF /2 encoding) is a
            // real, direct near-indirect call through a register -
            // same shape as I8086Codegen.cpp's own "call ax", just 32
            // bits wide.
            genExpr(*e.lhs);  // eax = the callee's own entry address
            code_ << "    call eax\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real i386 interrupt enable/disable - same role as
            // I8086Codegen.cpp's own `_ion()`/`_iof()` comment (that
            // file's own entry sequence runs with interrupts off from
            // `cli` until its own `sti`; this backend's stage-1 boot
            // never re-enables them at all before handing off to
            // stage-2, so `_ion()`/`_iof()` are the first real `sti`/
            // `cli` either way).
            code_ << "    " << (e.op == "ion" ? "sti" : "cli") << "\n";
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
    }
}

// slot[idxExpr]'s effective-address operand text, e.g.
// "[g_arr3+ebx]", with ebx already loaded with idx*4 (dwords are 4
// bytes on this target). See I8086Codegen.cpp's own
// `computeElementAddr` - identical role and reasoning, just 32 bits
// throughout (`shl eax,2` instead of `shl ax,1`).
std::string I386Codegen::computeElementAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    shl eax," << shift << "\n";
    code_ << "    mov ebx,eax\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    add ebx," << label(slot) << "\n";
        } else {
            // A Local array's words sit at decreasing addresses as
            // its own element index increases (addr(slot) is the
            // array's *first* word, closest to `ebp`; later words are
            // further below it - see I386Codegen.h's own `addr()`
            // comment) - the mirror image of a Global array's
            // increasing-address layout above, so this is base-minus-
            // scaled-index, not base-plus. Same shape as
            // I8086Codegen.cpp's own identical case.
            code_ << "    lea eax," << addr(slot) << "\n    sub eax,ebx\n    mov ebx,eax\n";
        }
    } else {
        code_ << "    add ebx," << operandOf(slot) << "\n";
    }
    return "[ebx]";
}

void I386Codegen::narrowEaxTo(Type to) {
    int w = widthOfType(to);
    if (w == 1) {
        code_ << "    " << (to.isUnsigned ? "movzx eax,al\n" : "movsx eax,al\n");
    } else if (w == 2) {
        code_ << "    " << (to.isUnsigned ? "movzx eax,ax\n" : "movsx eax,ax\n");
    }
    // 4 bytes (Int/Long): eax already holds the full value, nothing
    // to do. 8 bytes never reaches here - see genConvert()/the
    // isWide() branches in loadOperand()/storeOperand() below.
}

void I386Codegen::loadOperand(const std::string &mem, const std::string &highMem, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    " << (type.isUnsigned ? "movzx eax,byte " : "movsx eax,byte ") << mem << "\n";
    } else if (w == 2) {
        code_ << "    " << (type.isUnsigned ? "movzx eax,word " : "movsx eax,word ") << mem << "\n";
    } else if (w == 4) {
        code_ << "    mov eax," << mem << "\n";
    } else {  // 8 - long long: edx:eax, low dword first
        code_ << "    mov eax," << mem << "\n";
        code_ << "    mov edx," << highMem << "\n";
    }
}

void I386Codegen::storeOperand(const std::string &mem, const std::string &highMem, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    mov byte " << mem << ",al\n";
    } else if (w == 2) {
        code_ << "    mov word " << mem << ",ax\n";
    } else if (w == 4) {
        code_ << "    mov " << mem << ",eax\n";
    } else {  // 8
        code_ << "    mov " << mem << ",eax\n";
        code_ << "    mov " << highMem << ",edx\n";
    }
}

// An implicit or explicit type conversion (see IR.h's ExprKind::
// Convert). Every arithmetic/comparison operand narrower than `int`
// is already promoted by IRBuilder before any Binary/Unary ever sees
// it, so the only real work here is: crossing the native-int/`long
// long` boundary (sign/zero-extending eax into edx, or just dropping
// edx), or re-truncating+re-extending eax for a narrower target - see
// narrowEaxTo().
void I386Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);  // eax, or edx:eax if fromWide
    if (fromWide && !toWide) {
        narrowEaxTo(to);  // edx is simply discarded
    } else if (!fromWide && toWide) {
        code_ << (from.isUnsigned ? "    xor edx,edx\n" : "    cdq\n");
    } else if (!fromWide && !toWide) {
        narrowEaxTo(to);
    }
    // fromWide && toWide: the bit pattern is already correct (a
    // `long long`'s own signedness never changes its representation),
    // nothing to do.
}

// `long long` add/sub (lhs OP rhs): evaluate both into edx:eax,
// stashing lhs's two dwords on the stack while rhs evaluates, then
// ripple the carry/borrow across the dword pair with adc/sbb - the
// real-hardware generalization of a native add/sub one dword wider,
// same idiom any 32-on-32 long-long codegen uses.
void I386Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);                       // edx:eax = lhs
    code_ << "    push edx\n    push eax\n";  // stack (top->bottom): lhs_lo, lhs_hi
    genExpr(rhs);                       // edx:eax = rhs
    code_ << "    mov ebx,eax\n    mov ecx,edx\n";  // ebx=rhs_lo, ecx=rhs_hi
    code_ << "    pop eax\n    pop edx\n";    // eax=lhs_lo, edx=lhs_hi
    if (isAdd) {
        code_ << "    add eax,ebx\n    adc edx,ecx\n";
    } else {
        code_ << "    sub eax,ebx\n    sbb edx,ecx\n";
    }
}

// `long long` multiply, keeping the low 64 bits of the true 128-bit
// product: result_lo = lo(lhs)*lo(rhs) mod 2^32; result_hi =
// hi32(lo(lhs)*lo(rhs)) + lo(lhs)*hi(rhs) + hi(lhs)*lo(rhs), all mod
// 2^32 - the standard 32-on-32 cross-multiply decomposition, using
// the real hardware `mul` three times (each a genuine 32x32->64
// unsigned product) and discarding whatever each cross term's own
// `mul` leaves in `edx` (that overflow only ever affects bit 64 and
// beyond, which this type doesn't keep).
void I386Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push edx\n    push eax\n";  // lhs_lo, lhs_hi
    genExpr(rhs);
    code_ << "    mov edi,edx\n    mov esi,eax\n";  // edi=rhs_hi, esi=rhs_lo
    code_ << "    pop ebx\n    pop ecx\n";          // ebx=lhs_lo (top of stack), ecx=lhs_hi
    code_ << "    mov eax,ebx\n    mul edi\n";      // edx:eax = lhs_lo*rhs_hi
    code_ << "    push eax\n";                       // crossA_lo
    code_ << "    mov eax,ecx\n    mul esi\n";      // edx:eax = lhs_hi*rhs_lo
    code_ << "    push eax\n";                       // crossB_lo
    code_ << "    mov eax,ebx\n    mul esi\n";      // edx:eax = lhs_lo*rhs_lo (final result_lo/carry)
    code_ << "    pop ebx\n    pop ecx\n";          // ebx=crossB_lo, ecx=crossA_lo
    code_ << "    add edx,ebx\n    add edx,ecx\n";
}

void I386Codegen::genNegate64() {
    code_ << "    not eax\n    not edx\n";
    code_ << "    xor ecx,ecx\n";
    code_ << "    add eax,1\n";
    code_ << "    adc edx,ecx\n";
}

// Classic bit-serial shift/compare/subtract long division - see
// I386Codegen.h's own comment on this method. `ebx` addresses a
// 9-dword scratch block for the whole routine's duration: [ebx+0/4]
// the dividend being consumed (shifted left, MSB first), [ebx+8/12]
// the divisor (constant), [ebx+16/20] the remainder being built,
// [ebx+24/28] the quotient being built, [ebx+32] the iteration
// countdown. No native instruction here is wider than 32 bits, so
// every one of this type's own 64 bits is shifted/compared/
// subtracted through a pair of ordinary 32-bit operations, same
// spirit as genWideAdd()'s ripple-carry, just looped bit-by-bit since
// there's no `adc`-equivalent for an unknown-at-compile-time bit
// position.
//
// `isUnsigned` false divides *magnitudes*: lhs/rhs's own sign bits
// (read directly out of the pushed operand dwords, once they're
// sitting in stable memory - no register survives a nested genExpr()
// call, so this is simpler than trying to carry them through one)
// are stashed in the two scratch dwords this routine's own operand
// staging no longer needs once copied into n/d ([ebx+36]/[ebx+40]),
// each operand negated in place if its own sign bit was set, then
// restored afterward on the result per C's truncating-division rule:
// the quotient is negative iff the operands' signs differed, and the
// remainder always takes the dividend's own sign.
void I386Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push edx\n    push eax\n";  // lhs_hi, lhs_lo (lhs_lo on top)
    genExpr(rhs);
    code_ << "    push edx\n    push eax\n";  // rhs_hi, rhs_lo (rhs_lo on top)
    code_ << "    sub esp,36\n    mov ebx,esp\n";
    // Fetch the stashed operands from just above the scratch block.
    code_ << "    mov eax,[ebx+44]\n    mov [ebx+0],eax\n";   // n_lo = lhs_lo
    code_ << "    mov eax,[ebx+48]\n    mov [ebx+4],eax\n";   // n_hi = lhs_hi
    code_ << "    mov eax,[ebx+36]\n    mov [ebx+8],eax\n";   // d_lo = rhs_lo
    code_ << "    mov eax,[ebx+40]\n    mov [ebx+12],eax\n";  // d_hi = rhs_hi
    code_ << "    xor eax,eax\n";
    code_ << "    mov [ebx+16],eax\n    mov [ebx+20],eax\n";  // r = 0
    code_ << "    mov [ebx+24],eax\n    mov [ebx+28],eax\n";  // q = 0
    code_ << "    mov eax,64\n    mov [ebx+32],eax\n";        // 64 iterations

    std::string LskipNegN = newLabel(), LskipNegD = newLabel();
    if (!isUnsigned) {
        // lhsSign/rhsSign reuse [ebx+36]/[ebx+40] - the rhs_lo/rhs_hi
        // staging dwords, already copied into d_lo/d_hi above and
        // never read again for that purpose.
        code_ << "    mov eax,[ebx+4]\n    shr eax,31\n    mov [ebx+36],eax\n";  // lhsSign
        code_ << "    mov eax,[ebx+12]\n    shr eax,31\n    mov [ebx+40],eax\n";  // rhsSign
        code_ << "    mov eax,[ebx+36]\n    cmp eax,0\n    je " << LskipNegN << "\n";
        code_ << "    mov eax,[ebx+0]\n    mov edx,[ebx+4]\n";
        genNegate64();
        code_ << "    mov [ebx+0],eax\n    mov [ebx+4],edx\n";
        code_ << LskipNegN << ":\n";
        code_ << "    mov eax,[ebx+40]\n    cmp eax,0\n    je " << LskipNegD << "\n";
        code_ << "    mov eax,[ebx+8]\n    mov edx,[ebx+12]\n";
        genNegate64();
        code_ << "    mov [ebx+8],eax\n    mov [ebx+12],edx\n";
        code_ << LskipNegD << ":\n";
    }

    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LhiDiff = newLabel(), LsubYes = newLabel(), LsubNo = newLabel(), LshiftQ = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mov eax,[ebx+32]\n    cmp eax,0\n    je " << Ldone << "\n";
    code_ << "    sub eax,1\n    mov [ebx+32],eax\n";

    // n <<= 1 (0-filled), capturing its own old top bit into edx.
    code_ << "    mov eax,[ebx+4]\n    shr eax,31\n    mov edx,eax\n";  // edx = n's old top bit
    code_ << "    mov eax,[ebx+0]\n    shr eax,31\n    mov esi,eax\n";  // esi = n_lo's old top bit
    code_ << "    mov eax,[ebx+0]\n    shl eax,1\n    mov [ebx+0],eax\n";
    code_ << "    mov eax,[ebx+4]\n    shl eax,1\n    or eax,esi\n    mov [ebx+4],eax\n";

    // r = (r << 1) | (n's old top bit, already in edx).
    code_ << "    mov eax,[ebx+16]\n    shr eax,31\n    mov edi,eax\n";  // edi = r_lo's old top bit
    code_ << "    mov eax,[ebx+16]\n    shl eax,1\n    or eax,edx\n    mov [ebx+16],eax\n";
    code_ << "    mov eax,[ebx+20]\n    shl eax,1\n    or eax,edi\n    mov [ebx+20],eax\n";

    // if (r >= d) { r -= d; quotient bit = 1 } else { quotient bit = 0 }
    // (always an unsigned 64-bit magnitude compare, even for a signed
    // division - both n and d have already been negated to a
    // non-negative magnitude above.)
    code_ << "    mov eax,[ebx+20]\n    mov edx,[ebx+12]\n    cmp eax,edx\n    jne " << LhiDiff << "\n";
    code_ << "    mov eax,[ebx+16]\n    mov edx,[ebx+8]\n    cmp eax,edx\n    jae " << LsubYes << "\n";
    code_ << "    jmp " << LsubNo << "\n";
    code_ << LhiDiff << ":\n    jae " << LsubYes << "\n    jmp " << LsubNo << "\n";
    code_ << LsubYes << ":\n";
    code_ << "    mov eax,[ebx+16]\n    mov edx,[ebx+8]\n    sub eax,edx\n    mov [ebx+16],eax\n";
    code_ << "    mov eax,[ebx+20]\n    mov edx,[ebx+12]\n    sbb eax,edx\n    mov [ebx+20],eax\n";
    code_ << "    mov ecx,1\n    jmp " << LshiftQ << "\n";
    code_ << LsubNo << ":\n    mov ecx,0\n";
    code_ << LshiftQ << ":\n";

    // q = (q << 1) | quotient bit (in ecx).
    code_ << "    mov eax,[ebx+24]\n    shr eax,31\n    mov esi,eax\n";  // esi = q_lo's old top bit
    code_ << "    mov eax,[ebx+24]\n    shl eax,1\n    or eax,ecx\n    mov [ebx+24],eax\n";
    code_ << "    mov eax,[ebx+28]\n    shl eax,1\n    or eax,esi\n    mov [ebx+28],eax\n";
    code_ << "    jmp " << Lloop << "\n";

    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        // Quotient is negative iff the operands' signs differed;
        // the remainder always takes the dividend's (lhs) own sign -
        // see this method's own header comment.
        std::string LskipNegQ = newLabel(), LskipNegR = newLabel();
        code_ << "    mov ecx,[ebx+36]\n    mov eax,[ebx+40]\n    xor ecx,eax\n";
        code_ << "    cmp ecx,0\n    je " << LskipNegQ << "\n";
        code_ << "    mov eax,[ebx+24]\n    mov edx,[ebx+28]\n";
        genNegate64();
        code_ << "    mov [ebx+24],eax\n    mov [ebx+28],edx\n";
        code_ << LskipNegQ << ":\n";
        code_ << "    mov ecx,[ebx+36]\n    cmp ecx,0\n    je " << LskipNegR << "\n";
        code_ << "    mov eax,[ebx+16]\n    mov edx,[ebx+20]\n";
        genNegate64();
        code_ << "    mov [ebx+16],eax\n    mov [ebx+20],edx\n";
        code_ << LskipNegR << ":\n";
    }
    code_ << "    mov eax,[ebx+24]\n    mov edx,[ebx+28]\n";  // eax:edx = quotient
    code_ << "    mov esi,[ebx+16]\n    mov edi,[ebx+20]\n";  // esi:edi = remainder
    code_ << "    add esp,52\n";  // release the 9-dword scratch block and the 4 earlier stashed operand dwords
    if (wantRemainder) code_ << "    mov eax,esi\n    mov edx,edi\n";
}

// A `long long` shifted by a runtime count - no native instruction
// shifts more than 32 bits at once, so this is a counted loop of
// 1-bit shifts, each carrying the bit that crosses the dword boundary
// by hand (no `rcl`/`rcr` in this dialect) - same spirit as the
// divide routine's own bit-at-a-time loop above. `isUnsigned` only
// matters for `>>`: each step's own high-dword shift is `sar` (sign-
// preserving) or `shr` (zero-filling) accordingly; `<<` never cares
// about signedness.
void I386Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push edx\n    push eax\n";
    genExpr(rhs);                   // eax = shift count (already promoted to a plain int)
    code_ << "    mov ebx,eax\n";
    code_ << "    pop eax\n    pop edx\n";  // eax:edx restored = the value being shifted
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    cmp ebx,0\n    je " << Ldone << "\n    sub ebx,1\n";
    if (isLeft) {
        code_ << "    mov ecx,eax\n    shr ecx,31\n";  // ecx = eax's old top bit, carries into edx
        code_ << "    shl eax,1\n    shl edx,1\n    or edx,ecx\n";
    } else {
        code_ << "    mov ecx,edx\n    shl ecx,31\n";  // ecx = (edx&1)<<31, carries into eax's top
        code_ << "    shr eax,1\n    or eax,ecx\n";
        code_ << "    " << (isUnsigned ? "shr edx,1\n" : "sar edx,1\n");
    }
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// A `long long` comparison - compare the high dwords first (using
// the signed or unsigned jcc per `isUnsigned`, since that's where the
// sign bit lives); only if those are equal does the low dword's own
// comparison matter, and at that point it's always an unsigned
// compare regardless of the overall type's signedness (once the high
// halves match, the remaining 32 bits are a plain magnitude) - the
// standard big-number ordered-comparison idiom.
void I386Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push edx\n    push eax\n";
    genExpr(rhs);
    code_ << "    mov esi,eax\n    mov edi,edx\n";  // esi:edi = rhs (lo:hi)
    code_ << "    pop ebx\n    pop ecx\n";          // ebx:ecx = lhs (lo:hi)
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (op == "==" || op == "!=") {
        // Both branches below land on "equal"; == and != just want
        // opposite results for that outcome, so swap which literal
        // each label writes instead of duplicating the comparison.
        const char *onEqual = op == "==" ? "1" : "0";
        const char *onDiffer = op == "==" ? "0" : "1";
        code_ << "    cmp ebx,esi\n    jne " << Lfalse << "\n";
        code_ << "    cmp ecx,edi\n    jne " << Lfalse << "\n";
        code_ << "    mov eax," << onEqual << "\n    jmp " << Ldone << "\n";
        code_ << Lfalse << ":\n    mov eax," << onDiffer << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    std::string signedJcc = op == "<" ? "jl" : op == "<=" ? "jle" : op == ">" ? "jg" : "jge";
    std::string unsignedJcc = op == "<" ? "jb" : op == "<=" ? "jbe" : op == ">" ? "ja" : "jae";
    std::string hiJcc = isUnsigned ? unsignedJcc : signedJcc;
    std::string LhiDiff = newLabel();
    code_ << "    cmp ecx,edi\n    jne " << LhiDiff << "\n";
    code_ << "    cmp ebx,esi\n    " << unsignedJcc << " " << Ltrue << "\n";
    code_ << "    jmp " << Lfalse << "\n";
    code_ << LhiDiff << ":\n    " << hiJcc << " " << Ltrue << "\n";
    code_ << "    jmp " << Lfalse << "\n";
    code_ << Lfalse << ":\n    mov eax,0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov eax,1\n";
    code_ << Ldone << ":\n";
}

void I386Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        int byteOffset = static_cast<int>(i) * w;
        if (w == 8 && !elem.isSymbol) {
            // A long long array literal's own element - split across
            // both dwords; a symbol element (the dispatch-table
            // idiom, see AST.h's ArrayInitElem) always means a plain
            // native-int-width address instead, handled by the
            // general case below.
            auto v = static_cast<uint64_t>(elem.value);
            code_ << "    mov eax," << static_cast<uint32_t>(v) << "\n";
            code_ << "    mov " << addr(slot, byteOffset) << ",eax\n";
            code_ << "    mov eax," << static_cast<uint32_t>(v >> 32) << "\n";
            code_ << "    mov " << addr(slot, byteOffset + 4) << ",eax\n";
            continue;
        }
        code_ << "    mov eax," << arrayElemText(elem) << "\n";
        if (w == 1) {
            code_ << "    mov byte " << addr(slot, byteOffset) << ",al\n";
        } else if (w == 2) {
            code_ << "    mov word " << addr(slot, byteOffset) << ",ax\n";
        } else {
            code_ << "    mov " << addr(slot, byteOffset) << ",eax\n";
        }
    }
}

void I386Codegen::genIndex(const ir::Expr &e) {
    std::string memOperand = computeElementAddr(e.slot, *e.lhs);
    std::string highOperand = memOperand.substr(0, memOperand.size() - 1) + "+4]";
    loadOperand(memOperand, highOperand, e.slot->type);
}

void I386Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            code_ << "    mov eax," << label(operand.slot) << "\n";
        } else {
            code_ << "    lea eax," << addr(operand.slot) << "\n";
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genExpr(*operand.lhs);
        int shift = elemShift(operand.slot->type);
        if (shift > 0) code_ << "    shl eax," << shift << "\n";
        if (operand.slot->kind != ir::SlotKind::Global) {
            // See computeElementAddr()'s own identical comment - a
            // Local array's element address is base-minus-scaled-
            // index.
            code_ << "    mov ebx,eax\n    lea eax," << addr(operand.slot) << "\n    sub eax,ebx\n";
            return;
        }
        std::string lbl = label(operand.slot);
        if (operand.slot->arraySize > 0) {
            code_ << "    add eax," << lbl << "\n";
        } else {
            code_ << "    add eax,[" << lbl << "]\n";
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    mov eax," << label(operand.func) << "\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void I386Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here already
    // shares one common type (IRBuilder's own usual-arithmetic-
    // conversion pass - see Type.h) - `e.lhs->type` names it, except
    // for shift (where only the LHS's own type matters - see
    // IRBuilder.cpp's own Binary case) and &&/|| (width-independent
    // truth-testing, handled by genLogicalAnd/Or below).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+");
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push eax\n";
        genExpr(*e.rhs);
        code_ << "    pop ebx\n";
        if (op == "+") {
            code_ << "    add eax,ebx\n";
        } else {
            code_ << "    sub ebx,eax\n    mov eax,ebx\n";
        }
        return;
    }
    if (op == "*") {
        if (wide) {
            genWideMul(*e.lhs, *e.rhs);
        } else {
            genMul(*e.lhs, *e.rhs);
        }
        return;
    }
    if (op == "/" || op == "%") {
        if (wide) {
            genWideDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        } else {
            genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        }
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        if (wide) {
            genExpr(*e.lhs);
            code_ << "    push edx\n    push eax\n";
            genExpr(*e.rhs);
            code_ << "    mov ebx,eax\n    mov ecx,edx\n";
            code_ << "    pop eax\n    pop edx\n";
            code_ << "    " << insn << " eax,ebx\n    " << insn << " edx,ecx\n";
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push eax\n";
        genExpr(*e.rhs);
        code_ << "    pop ebx\n";
        code_ << "    " << insn << " eax,ebx\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push eax\n";
        genExpr(*e.rhs);
        code_ << "    mov ecx,eax\n";
        code_ << "    pop eax\n";
        const char *insn = op == "<<" ? "shl" : (isUnsigned ? "shr" : "sar");
        code_ << "    " << insn << " eax,cl\n";
        return;
    }
    if (op == "&&") {
        genLogicalAnd(*e.lhs, *e.rhs);
        return;
    }
    if (op == "||") {
        genLogicalOr(*e.lhs, *e.rhs);
        return;
    }
    genCompare(op, *e.lhs, *e.rhs);
}

void I386Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    bool isUnsigned = lhs.type.isUnsigned;
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, isUnsigned);
        return;
    }
    if (op == "==") return emitCompare(lhs, rhs, "je", isUnsigned);
    if (op == "!=") return emitCompare(lhs, rhs, "jne", isUnsigned);
    if (op == "<") return emitCompare(lhs, rhs, isUnsigned ? "jb" : "jl", isUnsigned);
    if (op == "<=") return emitCompare(lhs, rhs, isUnsigned ? "jbe" : "jle", isUnsigned);
    if (op == ">") return emitCompare(lhs, rhs, isUnsigned ? "ja" : "jg", isUnsigned);
    if (op == ">=") return emitCompare(lhs, rhs, isUnsigned ? "jae" : "jge", isUnsigned);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// cmp ebx,eax compares lhs-rhs (ebx=lhs, eax=rhs after the pop below)
// - see I8086Codegen.cpp's own `emitCompare` comment, unchanged here;
// `isUnsigned` is unused here (the caller already baked it into
// `jumpIfTrue`) but kept for symmetry with the wide path/call sites.
void I386Codegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue, bool) {
    genExpr(lhs);
    code_ << "    push eax\n";
    genExpr(rhs);
    code_ << "    pop ebx\n";
    code_ << "    cmp ebx,eax\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << jumpIfTrue << " " << Ltrue << "\n";
    code_ << "    mov eax,0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov eax,1\n";
    code_ << Ldone << ":\n";
}

// A wide (`long long`) operand's truthiness needs both dwords - `or
// eax,edx` folds them together before the usual "cmp eax,0" (the
// combined value is discarded immediately after, so clobbering eax
// this way is safe).
void I386Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    or eax,edx\n";
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp eax,0\n    je " << Lfalse << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or eax,edx\n";
    code_ << "    cmp eax,0\n    je " << Lfalse << "\n";
    code_ << "    jmp " << Ltrue << "\n";
    code_ << Lfalse << ":\n    mov eax,0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov eax,1\n";
    code_ << Ldone << ":\n";
}

void I386Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    or eax,edx\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp eax,0\n    jne " << Ltrue << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or eax,edx\n";
    code_ << "    cmp eax,0\n    jne " << Ltrue << "\n";
    code_ << "    jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n    mov eax,1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov eax,0\n";
    code_ << Ldone << ":\n";
}

void I386Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or eax,edx\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp eax,0\n    je " << Ltrue << "\n";
    code_ << "    mov eax,0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov eax,1\n";
    code_ << Ldone << ":\n";
}

// Real hardware multiply/divide (mul/imul/div/idiv) - a genuine
// 32x32->32 product/quotient, same real-hardware story as
// I8086Codegen.cpp's own `imul`/`idiv` comment, just 32 bits wide
// (`cdq` sign-extends eax into edx:eax, same role as that file's own
// `cwd`; the unsigned divide path needs no such extension - it just
// zeros edx instead, since there's no sign to carry).
void I386Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    bool isUnsigned = lhs.type.isUnsigned;
    genExpr(lhs);
    code_ << "    push eax\n";
    genExpr(rhs);
    code_ << "    pop ebx\n";
    code_ << "    " << (isUnsigned ? "mul" : "imul") << " ebx\n";  // edx:eax = eax*ebx; keep eax
}

void I386Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push eax\n";
    genExpr(rhs);
    code_ << "    mov ebx,eax\n";  // ebx = divisor
    code_ << "    pop eax\n";      // eax = dividend
    code_ << "    " << (isUnsigned ? "xor edx,edx\n" : "cdq\n");
    code_ << "    " << (isUnsigned ? "div" : "idiv") << " ebx\n";  // eax = quotient, edx = remainder
    if (wantRemainder) code_ << "    mov eax,edx\n";
}

std::string I386Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the i386 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> I386Codegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "raw") return assembleI386(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see I386Assembler.h's own `assembleI386Object`), and whichever
    // of these five names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to
    // serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleI386Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
