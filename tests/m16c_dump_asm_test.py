from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
RCC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else ROOT / "build/bin/rcc").resolve()
BUILD = ROOT / "build/m16c-dump-asm-tests"
# --dump-asm always writes to "build/<name><ext>" relative to rcc's own cwd
# (main.cpp's writeToBuildDir()), ignoring --out's directory - so these
# tests run rcc with cwd=ROOT and look for the dump there, not under BUILD.
ROOT_BUILD = ROOT / "build"


class M16cDumpAsmTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)

    def test_dump_asm_writes_a_file_for_a_single_compile(self):
        src = BUILD / "ret42.c"
        src.write_text("int main() { return 42; }\n")
        asm_out = ROOT_BUILD / "ret42.s16c"
        if asm_out.exists():
            asm_out.unlink()
        result = subprocess.run(
            [str(RCC), "--target=m16c", "--dump-asm", f"--out={BUILD / 'ret42.o'}", str(src)],
            capture_output=True, text=True, cwd=ROOT,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(asm_out.exists(), "expected --dump-asm to write " + str(asm_out))
        self.assertIn("fn_main", asm_out.read_text())
        asm_out.unlink()

    def test_dump_asm_splits_output_per_source_file_under_separate_compilation(self):
        helper_src = BUILD / "helper.c"
        helper_src.write_text("int helper() { return 7; }\n")
        main_src = BUILD / "caller.c"
        main_src.write_text("int helper(); int main() { return helper(); }\n")

        helper_asm = ROOT_BUILD / "helper.s16c"
        main_asm = ROOT_BUILD / "caller.s16c"
        for f in (helper_asm, main_asm):
            if f.exists():
                f.unlink()

        for src, out in ((helper_src, BUILD / "helper.o"), (main_src, BUILD / "caller.o")):
            result = subprocess.run(
                [str(RCC), "--target=m30626fjpfp", "--format=coff", "--dump-asm", f"--out={out}", str(src)],
                capture_output=True, text=True, cwd=ROOT,
            )
            self.assertEqual(result.returncode, 0, result.stderr)

        self.assertTrue(helper_asm.exists(), "expected --dump-asm to write " + str(helper_asm))
        self.assertTrue(main_asm.exists(), "expected --dump-asm to write " + str(main_asm))
        self.assertIn("fn_helper:", helper_asm.read_text())
        self.assertNotIn("fn_helper:", main_asm.read_text())
        self.assertIn("fn_main:", main_asm.read_text())
        helper_asm.unlink()
        main_asm.unlink()


if __name__ == "__main__":
    unittest.main()
