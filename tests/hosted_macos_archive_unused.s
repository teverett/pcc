.section __TEXT,__text,regular,pure_instructions
.globl _archive_unused
.p2align 2
_archive_unused:
    bl _missing_archive_only_symbol
    ret
