# 6502-atari2600

A "hello world" for the Atari 2600, built against `--target=atari2600`
(see `docs/platforms.md`'s own `atari2600` entry and
`src/codegen/m6502/M6502Codegen.h`'s header comment for why it's a
distinct target rather than another 6502 output format).

The 2600 has no OS, no character ROM, and no text display of any kind -
TIA, the video chip, only ever draws whatever the CPU pokes into its own
registers while the beam is actively scanning ("racing the beam"), which
needs cycle-exact timing no ordinary C-compiler codegen can promise. So
unlike `examples/6502-vic20/`, `examples/6502-c64/`, or
`examples/6502-atari400/` (all of which print text via an OS/KERNAL call),
there's no runtime library here and no display kernel - just `main.c`
poking a single, fixed color into `COLUBK` ($09, TIA's background-color
register, inline `asm { sta 9 }`) and letting the halt loop rcc generates
after `main()` returns hold it there. On real/emulated hardware this
paints the whole screen a solid color - the closest thing to a minimal
"it's alive" demo this chip has.

`make` links `main.c` straight into `build/main.a26`, a 4K cartridge
image (`--platform=atari2600-a26 --base='$F000'` - `$F000`/`$F800` are
the only two bases `--format=a26` accepts; see
`../../src/fileformat/atari2600/Atari2600RomFormat.h`). That file loads
directly in any 2600 emulator that reads a bare cartridge image (e.g.
Stella: File > Open ROM).

Since no TIA hardware is simulated in this project's own CI (`py65`'s
`MPU` is just the bare 6502 core), CI instead links the same program as
`build/main.raw` (`make build/main.raw`, identical bytes minus the
cartridge padding/vector table) and just checks that `$09` holds the
expected color byte after the program reaches its own halt loop - see
`test.py`'s own header comment.
