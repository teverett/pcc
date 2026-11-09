.text
.globl _main
.p2align 2
_main:
    adrp x9, _result@PAGE
    add x9, x9, _result@PAGEOFF
    mov w10, #42
    str w10, [x9]
    ldr w10, [x9]
    cmp w10, #42
    cset w0, ne
    ret

.section __DATA,__bss
.globl _result
.p2align 2
_result:
    .space 4
