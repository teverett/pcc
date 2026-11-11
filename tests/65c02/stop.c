// EXPECT: 17
//
// WDC 65C02's real stp (stop the clock until a hardware reset) - also
// only reachable through the `asm { }` escape hatch, same reasoning as
// bit_reset_set.c's own. Stores the result into `ac` first, then `stp`
// ends the program the same way bit_branch_wai.c's own `wai` does: the
// test harness's halt detection (two identical `pc` readings in a row)
// fires for `stp`'s permanent stall exactly the way it already does for
// the ordinary "halt: jmp halt" self-jump every target's own entry
// sequence ends with.
int main() {
    asm {
        ldai 17
        sta ac
        stzp ac+1
        stp
    }
}
