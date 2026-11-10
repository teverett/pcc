// EXPECT: 42
// An inline asm block, on each target its literal instruction text is
// actually written for, executes a real no-op without disturbing
// anything around it: the value computed right after the block is
// exactly what it would have been without it. Every other target
// skips straight to the same computation in plain C - asm text is
// inherently target-specific (see docs/language.md's "Inline
// asm"), so there's no portable instruction to spell here once.
int main() {
    int x;
    x = 40;
#ifdef __PDP7__
    asm { nop }
#elif defined(__8086__)
    asm { mov ax, ax }
#elif defined(__I386__)
    asm { mov eax, eax }
#elif defined(__ARMV7A__)
    asm { mov r0, r0 }
#elif defined(__ARMV8A__)
    asm { nop }
#endif
    x = x + 2;
    return x;  // 42
}
