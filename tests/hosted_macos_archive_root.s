.section __TEXT,__text,regular,pure_instructions
.globl _archive_root
.p2align 2
_archive_root:
    b _archive_leaf
