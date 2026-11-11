# example1

Compiles `hello.c` as a hosted executable for the current desktop OS, linking
against the host's C library/system library:

- macOS: `--target=armv8a-macos`, linked with `-lSystem`
- Linux: `--target=i386-linux`, linked with `-lc`
- Windows: `--target=i386-windows`, linked with `-lmsvcrt`

`make` builds for whichever OS it's run on; `make macos`, `make linux`, or
`make windows` build for a specific one.
