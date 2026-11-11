// EXPECT: 9
//
// WDC 65C02's real bbr0-7/bbs0-7 (branch if bit n of a zero-page byte is
// reset/set) and wai (wait for interrupt) - also only reachable through
// the `asm { }` escape hatch, same reasoning as bit_reset_set.c's own.
// Sets bit 3 of `ac`, confirms bbs3 takes the branch (bit set); clears
// it again, confirms bbr3 takes the branch (bit reset); either branch
// *not* being taken instead falls into the `fail:` marker path. Only
// once both branches are confirmed does it store the real result (9)
// before `wai` ends the program - the test harness's halt detection
// (two identical `pc` readings in a row) fires for `wai`'s permanent
// stall exactly the way it already does for the ordinary "halt: jmp
// halt" self-jump every target's own entry sequence ends with.
int main() {
    asm {
        stzp ac
        stzp ac+1
        smb3 ac
        bbs3 ac, bitwasset
        jmp fail
    bitwasset:
        rmb3 ac
        bbr3 ac, bitwasclear
        jmp fail
    bitwasclear:
        ldai 9
        sta ac
        wai
    fail:
        ldai 255
        sta ac
    }
}
