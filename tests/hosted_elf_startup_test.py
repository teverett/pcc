import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest


PCC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else "build/bin/pcc").resolve()


@unittest.skipIf(os.name == "nt", "Hosted ELF linking requires POSIX")
class HostedElfStartupTest(unittest.TestCase):
    def capture_startup(self, inputs):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            linker = root / "capture-linker"
            linker.write_text(
                "#!" + sys.executable + "\n"
                "import pathlib, shutil, sys\n"
                "args = sys.argv[1:]\n"
                "output = args[args.index('-o') + 1]\n"
                "startup = args[args.index('--dynamic-linker') + 2]\n"
                "shutil.copyfile(startup, pathlib.Path(__file__).with_name('startup.o'))\n"
                "pathlib.Path(output).write_bytes(b'captured')\n"
            )
            linker.chmod(0o755)
            result = subprocess.run(
                [str(PCC), "--link", "--target=i386", "--hosted", "--format=elf",
                 "--linker=" + str(linker), "--out=" + str(root / "app"),
                 "main.o", *inputs],
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            data = (root / "startup.o").read_bytes()
            self.assertEqual(data[:6], b"\x7fELF\x01\x01")
            section_offset = struct.unpack_from("<I", data, 32)[0]
            section_size, section_count, names_index = struct.unpack_from("<HHH", data, 46)
            sections = [
                struct.unpack_from("<IIIIIIIIII", data, section_offset + i * section_size)
                for i in range(section_count)
            ]
            names_section = sections[names_index]
            names = data[names_section[4]:names_section[4] + names_section[5]]
            for section in sections:
                start = section[0]
                if names[start:names.index(b"\0", start)] == b".text":
                    return data[section[4]:section[4] + section[5]], data
            self.fail("startup object has no .text section")

    def test_explicit_libc_selects_libc_startup(self):
        for inputs in (["-lc"], ["-l", "c"], ["-l:libc.so.6"],
                       ["/lib32/libc.so.6"], ["/lib32/libc.a"]):
            with self.subTest(inputs=inputs):
                code, obj = self.capture_startup(inputs)
                self.assertIn(b"__libc_start_main\0", obj)
                self.assertIn(b"main\0", obj)
                self.assertEqual(
                    code,
                    b"\xbd\0\0\0\0\x5e\x89\xe1\x81\xe4\xf0\xff\xff\xff"
                    b"\xb8\0\0\0\0\x50\x54\x52\x50\x50\x51\x56"
                    b"\xb8\0\0\0\0\x50\xe8\xfc\xff\xff\xff\xf4",
                )

    def test_libc_free_link_keeps_syscall_startup(self):
        for inputs in ([], ["-L", "/some/libc.so", "-lm"], ["-l:libcrypto.so"]):
            with self.subTest(inputs=inputs):
                code, obj = self.capture_startup(inputs)
                self.assertNotIn(b"__libc_start_main\0", obj)
                self.assertTrue(code.endswith(b"\xb8\x01\0\0\0\xcd\x80"))


if __name__ == "__main__":
    unittest.main()
