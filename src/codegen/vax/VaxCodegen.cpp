#include "VaxCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "VaxAssembler.h"
#include "fileformat/FileFormat.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing; the simulator sets the CPU's
// PC here directly. Must match VaxAssembler.h's kVaxBaseAddress.
constexpr uint32_t kBaseAddress = kVaxBaseAddress;

// Where the entry sequence points `sp` before calling main() - above the loaded program
// and its data, growing down into unused RAM.
constexpr uint32_t kInitialSP = 0x00100000u;

// A signed decimal literal for `imm`, from a 32-bit pattern.
std::string imm(uint32_t v) { return "imm " + std::to_string(static_cast<int32_t>(v)); }

// `Jcc` mnemonic for a comparison operator.
std::string jumpFor(const std::string &op, bool isUnsigned, int line) {
    if (op == "<") return isUnsigned ? "jlssu" : "jlss";
    if (op == ">") return isUnsigned ? "jgtru" : "jgtr";
    if (op == "<=") return isUnsigned ? "jlequ" : "jleq";
    if (op == ">=") return isUnsigned ? "jgequ" : "jgeq";
    if (op == "==") return "jeql";
    if (op == "!=") return "jneq";
    err(line, "internal: unhandled comparison operator '" + op + "'");
}
}  // namespace

std::string VaxCodegen::slotMem(const ir::Slot *slot) const { return slotMemAt(slot, 0); }

std::string VaxCodegen::slotMemAt(const ir::Slot *slot, int extra) const {
    if (slot->kind == ir::SlotKind::Global) {
        std::string m = "abs " + label(slot);
        if (extra > 0) m += "+" + std::to_string(extra);
        return m;
    }
    if (slot->kind == ir::SlotKind::Param) {
        return std::to_string(slot->frameOffset - 4 + extra) + "(ap)";
    }
    return std::to_string(-ir::localOffsetDescendingWide(*slot, typeWidths()) + extra) + "(fp)";
}

void VaxCodegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    movl imm " << label(slot) << "," << reg << "\n";
        return;
    }
    code_ << "    moval " << slotMem(slot) << "," << reg << "\n";
}

void VaxCodegen::loadImmediate(const std::string &reg, int32_t v) {
    if (v == 0) {
        code_ << "    clrl " << reg << "\n";
        return;
    }
    code_ << "    movl " << imm(static_cast<uint32_t>(v)) << "," << reg << "\n";
}

// A literal element's decimal text; a symbol element is the referenced Slot's or
// Function's label, written out bare - `dw` evaluates and relocates it as kVaxRelocAbs32.
std::string VaxCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

// Only called for a Global array - its storage is one fixed trailer blob, resolved once
// by the assembler/linker. A Local array gets a fresh frame slot per call instead, so its
// initializer is stored at runtime (see genLocalArrayInit()). `elemType`'s width picks
// `db`/`dh`/`dw` (a `long long` element, `w==8`, splits into two `dw`s: low word, high).
void VaxCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    const char *directive = w == 1 ? "db" : w == 2 ? "dh" : "dw";
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

// Stores a Local array's initializer into its freshly reserved frame slots, one element
// per instruction. Element i's address is the array's base, its lowest address, plus
// `w*i`, matching genIndexAddr()'s base-plus-scaled-index addressing.
void VaxCodegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        int at = w * static_cast<int>(i);
        if (elem.isSymbol) {
            const std::string target = elem.slot ? label(elem.slot) : label(elem.func);
            code_ << "    movl imm " << target << "," << slotMemAt(slot, at) << "\n";
            if (w == 8) code_ << "    clrl " << slotMemAt(slot, at + 4) << "\n";
            continue;
        }
        auto v = static_cast<uint64_t>(elem.value);
        if (w == 8) {
            code_ << "    movl " << imm(static_cast<uint32_t>(v)) << "," << slotMemAt(slot, at) << "\n";
            code_ << "    movl " << imm(static_cast<uint32_t>(v >> 32)) << "," << slotMemAt(slot, at + 4) << "\n";
        } else if (w == 4) {
            code_ << "    movl " << imm(static_cast<uint32_t>(v)) << "," << slotMemAt(slot, at) << "\n";
        } else if (w == 2) {
            code_ << "    movw imm " << (v & 0xFFFFu) << "," << slotMemAt(slot, at) << "\n";
        } else {
            code_ << "    movb imm " << (v & 0xFFu) << "," << slotMemAt(slot, at) << "\n";
        }
    }
}

void VaxCodegen::loadSlotValue(const ir::Slot *slot) { loadMem(slotMem(slot), slot->type); }
void VaxCodegen::storeSlotValue(const ir::Slot *slot) { storeMem(slotMem(slot), slot->type); }

std::string VaxCodegen::generate(const ir::Program &prog, bool objectMode) {
    perFileAsm_.clear();
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    std::string full;
    // Hands the text generated so far to the whole-program output and to the input file
    // it belongs to, leaving code_ empty for the next piece.
    auto flush = [&](const std::string &file) {
        std::string chunk = code_.str();
        code_.str("");
        code_.clear();
        full += chunk;
        perFileAsm_[file] += chunk;
    };

    // Real separate compilation means `prog` may not define `main` at all. In
    // `objectMode`, no object file emits this entry sequence itself - the linker
    // synthesizes exactly one once every object is merged and `main` is resolvable. An
    // object file's base is `org 0`; placing it is the linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        if (!mainFn) err(0, "no 'main' function defined");
        code_ << "_start:\n";
        code_ << "    movl " << imm(kInitialSP) << ",sp\n";
        code_ << "    calls imm 0,abs " << label(mainFn) << "\n";
        // The result sits in r0 the moment execution reaches `halt`.
        code_ << "done:\n    halt\n    brb done\n\n";
    }
    flush(mainFn ? mainFn->sourceFile : std::string());

    for (const auto &fb : prog.functionBodies) {
        genFunction(fb);
        flush(fb.func->sourceFile);
    }

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file - it must never get storage
        // here, only be referenced by name, letting the assembler defer it to a
        // relocation.
        if (slot->isExternal) continue;
        const size_t before = trailer_.size();
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w == 8) {
                auto uv = static_cast<uint64_t>(v);
                trailer_.push_back(lbl + ": dw " + std::to_string(static_cast<uint32_t>(uv)) + ", " +
                                    std::to_string(static_cast<uint32_t>(uv >> 32)) + "  ; " + slot->sourceName);
            } else {
                const char *directive = w == 1 ? "db" : w == 2 ? "dh" : "dw";
                trailer_.push_back(lbl + ": " + directive + " " +
                                    std::to_string(static_cast<int32_t>(static_cast<uint32_t>(v))) + "  ; " +
                                    slot->sourceName);
            }
        }
        for (size_t i = before; i < trailer_.size(); i++) perFileAsm_[slot->sourceFile] += trailer_[i] + "\n";
    }

    std::ostringstream out;
    out << full;
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

std::map<std::string, std::string> VaxCodegen::perFileAsm() const { return perFileAsm_; }

// Every function is entered with `calls` (see VaxCodegen.h): the 2-byte entry mask at the
// label is zero, so the prologue only has to reserve this call's Locals by lowering `sp`
// below the frame `calls` built at `fp`. Because every call gets its own frame on the
// hardware stack, direct and mutual recursion each get independent storage per call,
// limited only by the stack (kInitialSP).
void VaxCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    dh 0\n";
    if (fn.func->frameSize > 0) code_ << "    subl2 imm " << fn.func->frameSize << ",sp\n";
    genStmt(*fn.body);
    code_ << "    ret\n\n";
}

void VaxCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            if (isWide(s.cond->type)) code_ << "    bisl2 r1,r0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    tstl r0\n";
            code_ << "    jeql " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    jbr " << Ldone << "\n";
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
            if (isWide(s.cond->type)) code_ << "    bisl2 r1,r0\n";
            code_ << "    tstl r0\n";
            code_ << "    jeql " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    jbr " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    jbr " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    jbr " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            code_ << "    ret\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                storeSlotValue(s.declSlot);
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
        case ir::StmtKind::Label: {
            code_ << irLabel(s.labelId) << ":\n";
            return;
        }
        case ir::StmtKind::Goto: {
            code_ << "    jbr " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

void VaxCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                loadImmediate("r0", static_cast<int32_t>(static_cast<uint32_t>(v)));
                loadImmediate("r1", static_cast<int32_t>(static_cast<uint32_t>(v >> 32)));
                return;
            }
            loadImmediate("r0", static_cast<int32_t>(static_cast<uint32_t>(e.intValue)));
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotValue(e.slot);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // r2 = target address
                code_ << "    pushl r2\n";
                genExpr(*e.rhs);
                code_ << "    movl ai (sp),r2\n";
                storeMem("(r2)", e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            storeSlotValue(e.slot);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's width/signedness - the pointee type of a typed pointer,
            // or a plain int word for untyped-int-as-address. storeMem() handles a wide
            // pointee too, storing the whole r1:r0 quadword.
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    pushl r0\n";
            genExpr(*e.rhs);  // r0, or r1:r0 if wide
            code_ << "    movl ai (sp),r2\n";
            storeMem("(r2)", e.type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // r2 = address
                genCompoundThroughR2(e);
                return;
            }
            // Plain scalar target - slotMem() addresses it afresh with no side effect, so
            // there is no address to hold across `e.rhs`'s evaluation.
            genExpr(*e.rhs);  // r0 = rhs
            code_ << "    movl r0,r1\n";
            loadSlotValue(e.slot);  // r0 = old
            if (e.yieldOld) code_ << "    pushl r0\n";
            genCompoundOp(e.op, e.type);
            narrowR0To(e.type);
            storeSlotValue(e.slot);
            if (e.yieldOld) code_ << "    movl ai (sp),r0\n";
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    movl r0,r2\n";
            genCompoundThroughR2(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            if (isWide(e.lhs->type)) code_ << "    bisl2 r1,r0\n";
            code_ << "    tstl r0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    jeql " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    jbr " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    genNegate64("r0", "r1");
                } else {
                    code_ << "    mnegl r0,r0\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                // Loads at e.type's width/signedness - see DerefAssign above.
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    movl r0,r2\n";
                loadMem("(r2)", e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mcoml r0,r0\n";
                if (isWide(e.rhs->type)) code_ << "    mcoml r1,r1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Call: {
            // Pushed right-to-left, so argument 0 ends up closest to the stack top when
            // `calls` runs and sits at 4(ap) in the callee. A wide (`long long`)
            // argument pushes as one quadword, its low word at the lower address,
            // matching a wide Param's frameOffset layout. `calls` pops the arguments on
            // return, so the caller does no cleanup.
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    code_ << "    movq r0,-(sp)\n";
                    totalBytes += 8;
                } else {
                    code_ << "    pushl r0\n";
                    totalBytes += 4;
                }
            }
            if (totalBytes / 4 > 255) err(e.line, "too many call arguments for this target");
            code_ << "    calls imm " << totalBytes / 4 << ",abs " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - the callee isn't known until runtime, so
            // there's no declared parameter list to match pushed arguments against. The
            // address in r0 is a function label, which starts with its entry mask.
            genExpr(*e.lhs);
            code_ << "    calls imm 0,(r0)\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Func: {
            err(e.line, "internal: a bare function name has no value outside '&'");
        }
        case ir::ExprKind::Intrinsic: {
            // No interrupt vectors are set up on this bare-metal target, so interrupt
            // enable/disable is a no-op.
            (void)e;
            return;
        }
    }
}

// `r2` receives the computed address. `arraySize > 0` means the base is a compile-time
// known address (a Global's label, or a Local's `fp`-relative address - the array's first
// byte); otherwise `slot` is a plain pointer variable, so the base comes from reading
// that variable's own stored value.
//
// `elemShift(slot->type)` scales the index by the element width (1/2/4/8 bytes). A
// Global or Local array's elements sit at increasing addresses as the index grows
// (`base+i*w`), a Local array's base being its lowest address, element 0.
void VaxCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    ashl imm " << shift << ",r0,r0\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    addl3 imm " << label(slot) << ",r0,r2\n";
        } else {
            code_ << "    moval " << slotMem(slot) << ",r2\n";
            code_ << "    addl2 r0,r2\n";
        }
    } else {
        code_ << "    addl3 r0," << slotMem(slot) << ",r2\n";  // base = the pointer variable's value
    }
}

void VaxCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadMem("(r2)", e.slot->type);
}

void VaxCodegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    addl2 r1,r0\n"; return; }
    if (op == "-") { code_ << "    subl2 r1,r0\n"; return; }
    if (op == "*") { code_ << "    mull2 r1,r0\n"; return; }
    if (op == "&") { code_ << "    mcoml r1,r3\n    bicl2 r3,r0\n"; return; }
    if (op == "|") { code_ << "    bisl2 r1,r0\n"; return; }
    if (op == "^") { code_ << "    xorl2 r1,r0\n"; return; }
    if (op == "<<") { code_ << "    ashl r1,r0,r0\n"; return; }
    if (op == ">>") {
        if (isUnsigned) {
            code_ << "    subl3 r1,imm 32,r3\n    extzv r1,r3,r0,r0\n";
        } else {
            code_ << "    mnegl r1,r3\n    ashl r3,r0,r0\n";
        }
        return;
    }
    // "/" or "%": the dividend is `old` (r0) and the divisor `rhs` (r1); swap them into
    // the dividend-in-r1, divisor-in-r0 shape genDivMod()'s sequences expect.
    code_ << "    movl r0,r3\n    movl r1,r0\n    movl r3,r1\n";
    if (isUnsigned) {
        genUnsignedDivMod(op == "%");
    } else if (op == "%") {
        code_ << "    divl3 r0,r1,r3\n    mull3 r3,r0,r3\n    subl3 r3,r1,r0\n";
    } else {
        code_ << "    divl3 r0,r1,r0\n";
    }
}

// r2 must already hold the resolved address on entry; it is stashed on the stack across
// `e.rhs`'s evaluation, which can clobber it. genCompoundOp() itself never touches r2.
void VaxCodegen::genCompoundThroughR2(const ir::Expr &e) {
    code_ << "    pushl r2\n";
    genExpr(*e.rhs);                     // r0 = rhs
    code_ << "    movl r0,r1\n";         // r1 = rhs
    code_ << "    movl ai (sp),r2\n";    // r2 = address
    loadMem("(r2)", e.type);             // r0 = old
    if (e.yieldOld) code_ << "    pushl r0\n";
    genCompoundOp(e.op, e.type);         // r0 = new (raw)
    narrowR0To(e.type);
    storeMem("(r2)", e.type);
    if (e.yieldOld) code_ << "    movl ai (sp),r0\n";
}

void VaxCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadSlotAddr("r0", operand.slot);
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    movl r2,r0\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    movl imm " << label(operand.func) << ",r0\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void VaxCodegen::loadMem(const std::string &mem, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    " << (type.isUnsigned ? "movzbl" : "cvtbl") << " " << mem << ",r0\n";
    } else if (w == 2) {
        code_ << "    " << (type.isUnsigned ? "movzwl" : "cvtwl") << " " << mem << ",r0\n";
    } else if (w == 4) {
        code_ << "    movl " << mem << ",r0\n";
    } else {  // 8 - long long: r1:r0 (hi:lo), low word first
        code_ << "    movq " << mem << ",r0\n";
    }
}

void VaxCodegen::storeMem(const std::string &mem, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    movb r0," << mem << "\n";
    } else if (w == 2) {
        code_ << "    movw r0," << mem << "\n";
    } else if (w == 4) {
        code_ << "    movl r0," << mem << "\n";
    } else {
        code_ << "    movq r0," << mem << "\n";
    }
}

void VaxCodegen::narrowR0To(Type to) {
    int w = widthOfType(to);
    if (w == 1) {
        code_ << "    " << (to.isUnsigned ? "movzbl" : "cvtbl") << " r0,r0\n";
    } else if (w == 2) {
        code_ << "    " << (to.isUnsigned ? "movzwl" : "cvtwl") << " r0,r0\n";
    }
    // 4 bytes (Int/Long): r0 already holds the full value. 8 never reaches here.
}

// An implicit or explicit type conversion. Every arithmetic/comparison operand narrower
// than `int` is already promoted by IRBuilder, so the only work here is crossing the
// int/`long long` boundary (sign/zero-extending r0 into r1, or dropping r1), or
// re-truncating+re-extending r0 for a narrower target.
void VaxCodegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);  // r0, or r1:r0 if fromWide
    if (fromWide && !toWide) {
        narrowR0To(to);  // r1 is simply discarded
    } else if (!fromWide && toWide) {
        if (from.isUnsigned) {
            code_ << "    clrl r1\n";
        } else {
            code_ << "    ashl imm -31,r0,r1\n";  // sign-extend r0 into r1
        }
    } else if (!fromWide && !toWide) {
        narrowR0To(to);
    }
    // fromWide && toWide: the bit pattern is already correct.
}

// `long long` add/sub (lhs OP rhs). The lhs quadword is pushed, so the low word is the
// first longword popped and the carry/borrow `addl2`/`subl2` leave feeds the high-word
// `adwc`/`sbwc`.
void VaxCodegen::genWideAddSub(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);
    code_ << "    movq r0,-(sp)\n";
    genExpr(rhs);  // r1:r0 = rhs
    if (isAdd) {
        code_ << "    addl2 ai (sp),r0\n";
        code_ << "    adwc ai (sp),r1\n";
    } else {
        code_ << "    movq ai (sp),r2\n";  // r3:r2 = lhs (hi:lo)
        code_ << "    subl2 r0,r2\n";
        code_ << "    sbwc r1,r3\n";
        code_ << "    movq r2,r0\n";
    }
}

// `long long` multiply, keeping the low 64 bits of the true product. `emul` multiplies two
// signed longwords into a quadword; the low words are really unsigned, so each operand
// with its top bit set understates the product's high word by the other operand, which
// is added back. The two cross products only affect the high word, so a plain 32-bit
// multiply of each suffices.
void VaxCodegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    movq r0,-(sp)\n";
    genExpr(rhs);                         // r1:r0 = rhs (hi:lo)
    code_ << "    movq ai (sp),r2\n";     // r3:r2 = lhs (hi:lo)
    std::string La = newLabel(), Lb = newLabel();
    code_ << "    emul r2,r0,imm 0,r4\n";  // r5:r4 = signed lhs_lo * rhs_lo
    code_ << "    tstl r2\n    jgeq " << La << "\n    addl2 r0,r5\n" << La << ":\n";
    code_ << "    tstl r0\n    jgeq " << Lb << "\n    addl2 r2,r5\n" << Lb << ":\n";
    code_ << "    mull3 r2,r1,r6\n    addl2 r6,r5\n";  // lhs_lo * rhs_hi
    code_ << "    mull3 r3,r0,r6\n    addl2 r6,r5\n";  // lhs_hi * rhs_lo
    code_ << "    movq r4,r0\n";
}

void VaxCodegen::genNegate64(const std::string &lo, const std::string &hi) {
    code_ << "    mcoml " << lo << "," << lo << "\n    mcoml " << hi << "," << hi << "\n";
    code_ << "    addl2 imm 1," << lo << "\n    adwc imm 0," << hi << "\n";
}

// Classic bit-serial shift/compare/subtract long division, entirely in registers:
//   r3:r2 = dividend n,  r5:r4 = divisor d,  r7:r6 = remainder,  r9:r8 = quotient,
//   r10 = bit counter,   r11 = sign flags (signed only),  r1 = remainder-overflow bit.
// `isUnsigned` false divides magnitudes: each negative operand is negated first (flags
// bit 0 = dividend negative, bit 1 = divisor negative), then the quotient is negated if
// the signs differ and the remainder if the dividend was negative, per C's truncating
// division rule.
//
// The remainder can reach 2^63 or more only for an unsigned divisor that large, in which
// case the shifted remainder needs 65 bits: its lost top bit (saved in r1 before the
// shift) means the value is certainly >= d, forcing the subtract.
void VaxCodegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    movq r0,-(sp)\n";
    genExpr(rhs);
    code_ << "    movq r0,r4\n";           // d
    code_ << "    movq ai (sp),r2\n";      // n

    if (!isUnsigned) {
        std::string LposN = newLabel(), LposD = newLabel();
        code_ << "    clrl r11\n";
        code_ << "    tstl r3\n    jgeq " << LposN << "\n    bisl2 imm 1,r11\n";
        genNegate64("r2", "r3");
        code_ << LposN << ":\n";
        code_ << "    tstl r5\n    jgeq " << LposD << "\n    bisl2 imm 2,r11\n";
        genNegate64("r4", "r5");
        code_ << LposD << ":\n";
    }

    std::string Lloop = newLabel(), Lyes = newLabel(), Lno = newLabel(), Lnext = newLabel();
    code_ << "    clrq r6\n    clrq r8\n    movl imm 64,r10\n";
    code_ << Lloop << ":\n";
    code_ << "    extzv imm 31,imm 1,r7,r1\n";  // remainder's top bit, lost by the shift below
    code_ << "    extzv imm 31,imm 1,r3,r0\n";  // dividend's top bit, shifted into the remainder
    code_ << "    ashq imm 1,r2,r2\n";
    code_ << "    ashq imm 1,r6,r6\n";
    code_ << "    bisl2 r0,r6\n";
    code_ << "    tstl r1\n    jneq " << Lyes << "\n";
    code_ << "    cmpl r7,r5\n    jlssu " << Lno << "\n    jgtru " << Lyes << "\n";
    code_ << "    cmpl r6,r4\n    jlssu " << Lno << "\n";
    code_ << Lyes << ":\n";
    code_ << "    subl2 r4,r6\n    sbwc r5,r7\n";
    code_ << "    ashq imm 1,r8,r8\n    incl r8\n    jbr " << Lnext << "\n";
    code_ << Lno << ":\n";
    code_ << "    ashq imm 1,r8,r8\n";
    code_ << Lnext << ":\n";
    code_ << "    decl r10\n    jgtr " << Lloop << "\n";

    if (!isUnsigned) {
        std::string LkeepQ = newLabel(), LkeepR = newLabel();
        code_ << "    tstl r11\n    jeql " << LkeepQ << "\n    cmpl r11,imm 3\n    jeql " << LkeepQ << "\n";
        genNegate64("r8", "r9");
        code_ << LkeepQ << ":\n";
        code_ << "    bitl imm 1,r11\n    jeql " << LkeepR << "\n";
        genNegate64("r6", "r7");
        code_ << LkeepR << ":\n";
    }
    code_ << "    movq " << (wantRemainder ? "r6" : "r8") << ",r0\n";
}

// A `long long` shifted by a runtime count (already promoted to a plain int). `ashq`
// shifts arithmetically; a logical right shift then clears the sign-filled top bits with
// a mask built by shifting all-ones left by `64 - count` (a count of 0 shifts by 64, which
// `ashq` defines as clearing everything, leaving an empty mask).
void VaxCodegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    movq r0,-(sp)\n";
    genExpr(rhs);  // r0 = shift count
    code_ << "    movl r0,r4\n";
    code_ << "    movq ai (sp),r0\n";  // r1:r0 = the value being shifted
    if (isLeft) {
        code_ << "    ashq r4,r0,r0\n";
        return;
    }
    code_ << "    mnegl r4,r5\n    ashq r5,r0,r0\n";
    if (!isUnsigned) return;
    code_ << "    subl3 r4,imm 64,r6\n";
    code_ << "    mnegl imm 1,r8\n    movl r8,r9\n    ashq r6,r8,r8\n";
    code_ << "    bicl2 r8,r0\n    bicl2 r9,r1\n";
}

// A `long long` comparison - compare the high words first (signed or unsigned per
// `isUnsigned`), falling back to an unsigned compare of the low words only when the high
// words are equal.
void VaxCodegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    movq r0,-(sp)\n";
    genExpr(rhs);                      // r1:r0 = rhs (hi:lo)
    code_ << "    movq ai (sp),r2\n";  // r3:r2 = lhs (hi:lo)
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (op == "==" || op == "!=") {
        std::string Lne = newLabel();
        code_ << "    cmpl r2,r0\n    jneq " << Lne << "\n    cmpl r3,r1\n    jneq " << Lne << "\n";
        code_ << (op == "==" ? "    movl imm 1,r0\n" : "    clrl r0\n") << "    jbr " << Ldone << "\n";
        code_ << Lne << ":\n" << (op == "==" ? "    clrl r0\n" : "    movl imm 1,r0\n");
        code_ << Ldone << ":\n";
        return;
    }
    // Once the high words are equal, the remaining 32 bits are a plain unsigned magnitude.
    std::string LhiDiff = newLabel();
    code_ << "    cmpl r3,r1\n    jneq " << LhiDiff << "\n";
    code_ << "    cmpl r2,r0\n    " << jumpFor(op, true, lhs.line) << " " << Ltrue << "\n    jbr " << Lfalse << "\n";
    code_ << LhiDiff << ":\n    " << jumpFor(op, isUnsigned, lhs.line) << " " << Ltrue << "\n";
    code_ << Lfalse << ":\n    clrl r0\n    jbr " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl imm 1,r0\n";
    code_ << Ldone << ":\n";
}

void VaxCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand already shares one common type;
    // `e.lhs->type` names it, except for shift (only the LHS's type matters) and &&/||
    // (width-independent, handled by genLogicalAnd/Or below).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAddSub(*e.lhs, *e.rhs, op == "+");
            return;
        }
        genExpr(*e.lhs);
        code_ << "    pushl r0\n";
        genExpr(*e.rhs);  // r0 = rhs, the lhs is on the stack
        if (op == "+") {
            code_ << "    addl2 ai (sp),r0\n";
        } else {
            code_ << "    subl3 r0,ai (sp),r0\n";  // r0 = lhs - rhs
        }
        return;
    }
    if (op == "*") {
        if (wide) {
            genWideMul(*e.lhs, *e.rhs);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    pushl r0\n";
        genExpr(*e.rhs);
        code_ << "    mull2 ai (sp),r0\n";
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
        genExpr(*e.lhs);
        if (wide) {
            code_ << "    movq r0,-(sp)\n";
            genExpr(*e.rhs);
            code_ << "    movq ai (sp),r2\n";  // r3:r2 = lhs
            if (op == "&") {
                code_ << "    mcoml r0,r0\n    mcoml r1,r1\n    bicl3 r0,r2,r0\n    bicl3 r1,r3,r1\n";
            } else {
                const char *insn = op == "|" ? "bisl2" : "xorl2";
                code_ << "    " << insn << " r2,r0\n    " << insn << " r3,r1\n";
            }
            return;
        }
        code_ << "    pushl r0\n";
        genExpr(*e.rhs);
        if (op == "&") {
            code_ << "    mcoml r0,r0\n    bicl3 r0,ai (sp),r0\n";  // lhs & ~(~rhs)
        } else {
            code_ << "    " << (op == "|" ? "bisl2" : "xorl2") << " ai (sp),r0\n";
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    pushl r0\n";
        genExpr(*e.rhs);  // r0 = shift count
        code_ << "    movl r0,r1\n    movl ai (sp),r0\n";  // r0 = value to shift, r1 = count
        genCompoundOp(op, operandType);
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

// Hardware divide covers the signed case only. `%` is synthesized from the quotient:
// dividend - quotient * divisor.
void VaxCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    pushl r0\n";
    genExpr(rhs);
    code_ << "    movl ai (sp),r1\n";  // r1 = dividend, r0 = divisor
    if (isUnsigned) {
        genUnsignedDivMod(wantRemainder);
        return;
    }
    if (!wantRemainder) {
        code_ << "    divl3 r0,r1,r0\n";
        return;
    }
    code_ << "    divl3 r0,r1,r3\n    mull3 r3,r0,r3\n    subl3 r3,r1,r0\n";
}

// `divl3` is signed, so an unsigned divide splits by the divisor's and dividend's top bits:
//   divisor >= 2^31: the quotient is 1 if dividend >= divisor, else 0.
//   dividend < 2^31: an ordinary signed divide is exact.
//   otherwise: divide the dividend halved by the divisor, double that, and correct the
//   remainder - which the halving can leave up to one divisor too large - by one more
//   subtract of the divisor if it reached it.
void VaxCodegen::genUnsignedDivMod(bool wantRemainder) {
    std::string Lbig = newLabel(), Lhigh = newLabel(), Lq = newLabel();
    code_ << "    tstl r0\n    jlss " << Lbig << "\n";
    code_ << "    tstl r1\n    jlss " << Lhigh << "\n";
    code_ << "    divl3 r0,r1,r3\n    jbr " << Lq << "\n";
    code_ << Lhigh << ":\n";
    code_ << "    extzv imm 1,imm 31,r1,r4\n";
    code_ << "    divl3 r0,r4,r3\n    ashl imm 1,r3,r3\n";
    code_ << "    mull3 r3,r0,r4\n    subl3 r4,r1,r4\n";
    code_ << "    cmpl r4,r0\n    jlssu " << Lq << "\n    incl r3\n    jbr " << Lq << "\n";
    code_ << Lbig << ":\n";
    code_ << "    clrl r3\n    cmpl r1,r0\n    jlssu " << Lq << "\n    movl imm 1,r3\n";
    code_ << Lq << ":\n";
    if (wantRemainder) {
        code_ << "    mull3 r3,r0,r4\n    subl3 r4,r1,r0\n";
    } else {
        code_ << "    movl r3,r0\n";
    }
}

// VAX has no compare-and-set instruction - a comparison needs `cmpl` plus a conditional
// branch to decide which boolean value to materialize. `isUnsigned` picks the plain or
// unsigned branch family for ordered comparisons; `==`/`!=` need no such choice.
void VaxCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, lhs.type.isUnsigned);
        return;
    }
    genExpr(lhs);
    code_ << "    pushl r0\n";
    genExpr(rhs);
    code_ << "    cmpl ai (sp),r0\n";  // flags for lhs - rhs
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << jumpFor(op, lhs.type.isUnsigned, lhs.line) << " " << Ltrue << "\n";
    code_ << "    clrl r0\n";
    code_ << "    jbr " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl imm 1,r0\n";
    code_ << Ldone << ":\n";
}

void VaxCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    bisl2 r1,r0\n";
    std::string Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    tstl r0\n    jeql " << Lfalse << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    bisl2 r1,r0\n";
    code_ << "    tstl r0\n    jeql " << Lfalse << "\n";
    code_ << "    movl imm 1,r0\n    jbr " << Ldone << "\n";
    code_ << Lfalse << ":\n    clrl r0\n";
    code_ << Ldone << ":\n";
}

void VaxCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    bisl2 r1,r0\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    tstl r0\n    jneq " << Ltrue << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    bisl2 r1,r0\n";
    code_ << "    tstl r0\n    jneq " << Ltrue << "\n";
    code_ << "    clrl r0\n    jbr " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl imm 1,r0\n";
    code_ << Ldone << ":\n";
}

void VaxCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    bisl2 r1,r0\n";
    std::string Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    tstl r0\n    jneq " << Lfalse << "\n";
    code_ << "    movl imm 1,r0\n    jbr " << Ldone << "\n";
    code_ << Lfalse << ":\n    clrl r0\n";
    code_ << Ldone << ":\n";
}

std::optional<std::string> VaxCodegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "raw") return assembleVax(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: builds one format-neutral ObjectFile, then
    // picks that format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleVaxObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace rcc
