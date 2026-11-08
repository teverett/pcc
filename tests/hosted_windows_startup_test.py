import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest


PCC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else "build/bin/pcc").resolve()


@unittest.skipIf(os.name == "nt", "The capture linker uses a POSIX executable script")
class HostedWindowsStartupTest(unittest.TestCase):
    def check_startup(self, linker_kind):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "main.c"
            source.write_text("int main() { return 37; }\n")
            obj = root / "main.obj"
            output = root / "app.exe"
            capture = root / "startup.obj"
            linker = root / "capture-linker"
            linker.write_text(
                "#!" + sys.executable + "\n"
                "import json, pathlib, shutil, sys\n"
                "args = sys.argv[1:]\n"
                "pathlib.Path(__file__).with_name('args.json').write_text(json.dumps(args))\n"
                "if '-o' in args:\n"
                "    i = args.index('-o')\n"
                "    output, startup = args[i + 1:i + 3]\n"
                "else:\n"
                "    i = next(i for i, arg in enumerate(args) if arg.startswith('/out:'))\n"
                "    output, startup = args[i][5:], args[i + 1]\n"
                "shutil.copyfile(startup, pathlib.Path(__file__).with_name('startup.obj'))\n"
                "pathlib.Path(output).write_bytes(b'captured')\n"
            )
            linker.chmod(0o755)
            compile_result = subprocess.run(
                [str(PCC), "--target=i386", "--hosted=windows", "--format=coff",
                 "--out=" + str(obj), str(source)],
                capture_output=True, text=True,
            )
            self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
            result = subprocess.run(
                [str(PCC), "--link", "--target=i386", "--hosted=windows", "--format=coff",
                 "--windows-linker=" + linker_kind, "--linker=" + str(linker),
                 "--out=" + str(output), str(obj)],
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(output.read_bytes(), b"captured")
            data = capture.read_bytes()
            machine, _, _, symbols_offset, _, _, _ = struct.unpack_from("<HHIIIHH", data)
            self.assertEqual(machine, 0x014C)
            section = struct.unpack_from("<8sIIIIIIHHI", data, 20)
            self.assertEqual(section[0].rstrip(b"\0"), b".text")
            code = data[section[4]:section[4] + section[3]]
            self.assertEqual(
                code,
                b"\xb8\0\0\0\0\x50\x50\xe8\0\0\0\0\x81\xc4\x08\0\0\0"
                b"\x50\xe8\0\0\0\0",
            )
            self.assertEqual(section[7], 2)
            names = []
            for index, expected_offset in enumerate((8, 20)):
                offset, symbol_index, kind = struct.unpack_from(
                    "<IIH", data, section[5] + index * 10,
                )
                self.assertEqual(offset, expected_offset)
                self.assertEqual(kind, 0x0014)
                symbol = symbols_offset + symbol_index * 18
                name = data[symbol:symbol + 8]
                if name[:4] == b"\0" * 4:
                    string_offset = struct.unpack_from("<I", name, 4)[0]
                    symbol_count = struct.unpack_from("<I", data, 12)[0]
                    start = symbols_offset + symbol_count * 18 + string_offset
                    name = data[start:data.index(b"\0", start)]
                names.append(name.rstrip(b"\0"))
            self.assertEqual(names, [b"_main", b"_ExitProcess@4"])
            return json.loads((root / "args.json").read_text())

    def test_mingw_startup_arguments_and_exit_code(self):
        self.check_startup("mingw")

    def test_msvc_startup_arguments_and_exit_code(self):
        self.check_startup("msvc")

    def test_msvc_disables_safeseh_for_pcc_objects(self):
        args = self.check_startup("msvc")
        self.assertEqual([arg.lower() for arg in args].count("/safeseh:no"), 1)

    def test_mingw_does_not_receive_msvc_safeseh_option(self):
        args = self.check_startup("mingw")
        self.assertNotIn("/safeseh:no", [arg.lower() for arg in args])


if __name__ == "__main__":
    unittest.main()
