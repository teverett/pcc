from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
RCC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else ROOT / "build/bin/rcc").resolve()
BUILD = ROOT / "build/m6809-link-tests"


# Regression test for a bug where `--target=6809`'s COFF machine ID (0x6809)
# was missing from FileFormat.cpp's detectFileFormat() allowlist, so every
# `--link` step silently mis-sniffed a 6809 object as a Raw binary instead of
# COFF - corrupting the symbol table before GenericLinker ever saw it, and
# surfacing as a misleading "undefined symbol" error naming the wrong symbol.
class M6809LinkTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)

    def compile(self, stem, source):
        source_path = BUILD / f"{stem}.c"
        object_path = BUILD / f"{stem}.o"
        source_path.write_text(source)
        result = subprocess.run(
            [str(RCC), "--target=6809", "--format=coff", f"--out={object_path}", str(source_path)],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return object_path

    def test_coff_object_is_detected_as_coff_not_raw(self):
        # The bug's root cause, directly: a 6809 object's first two bytes are
        # its little-endian COFF machine ID (0x6809 -> 09 68), which must be
        # in detectFileFormat()'s known-machines set.
        obj = self.compile("sniff", "int main() { return 1; }\n")
        data = obj.read_bytes()
        self.assertEqual(data[0], 0x09)
        self.assertEqual(data[1], 0x68)

    def test_cross_object_call_links_successfully(self):
        # The bug's symptom: linking two objects where main.o calls a
        # function defined in lib.o used to fail with a misleading
        # "undefined symbol 'fn_main'" (main.o's own object misread as Raw).
        main_obj = self.compile("main", "int helper();\nint main() {\n    return helper();\n}\n")
        lib_obj = self.compile("lib", "int helper() {\n    return 42;\n}\n")
        output = BUILD / "out.raw"
        result = subprocess.run(
            [str(RCC), "--link", "--target=6809", "--format=raw", f"--out={output}", str(main_obj), str(lib_obj)],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertGreater(output.stat().st_size, 0)

    def test_single_object_with_genuinely_undefined_symbol_names_it_correctly(self):
        # Without lib.o, `helper` really is undefined - this must fail, but
        # naming the actual missing symbol, not some other one (the bug's
        # tell: it named 'fn_main', which IS defined, instead of 'fn_helper').
        main_obj = self.compile("main_alone", "int helper();\nint main() {\n    return helper();\n}\n")
        output = BUILD / "out_alone.raw"
        result = subprocess.run(
            [str(RCC), "--link", "--target=6809", "--format=raw", f"--out={output}", str(main_obj)],
            capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("fn_helper", result.stderr)
        self.assertNotIn("fn_main", result.stderr)


if __name__ == "__main__":
    unittest.main()
