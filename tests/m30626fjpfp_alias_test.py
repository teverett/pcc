from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
PCC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else ROOT / "build/bin/pcc").resolve()
BUILD = ROOT / "build/m30626fjpfp-alias-tests"

SOURCE = """
int main() {
    return 42;
}
"""

MACRO_SOURCE = """
#ifdef __M30626FJPFP__
int main() { return 1; }
#else
int main() { return 0; }
#endif
"""


class M30626fjpfpAliasTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.source_path = BUILD / "ret42.c"
        cls.source_path.write_text(SOURCE)
        cls.macro_source_path = BUILD / "macro_check.c"
        cls.macro_source_path.write_text(MACRO_SOURCE)

    def test_m30626fjpfp_output_matches_m16c_byte_for_byte(self):
        # m30626fjpfp is a software-identical alias for the m16c backend -
        # the Renesas M30626FJPFP's real hardware differences (on-chip
        # peripherals, memory size) are invisible to any instruction this
        # compiler emits, so both targets must produce identical assembly.
        m16c_out = BUILD / "ret42_m16c.s16c"
        m30626_out = BUILD / "ret42_m30626fjpfp.s16c"
        for target, out in (("m16c", m16c_out), ("m30626fjpfp", m30626_out)):
            result = subprocess.run(
                [str(PCC), f"--target={target}", "--format=asm", f"--out={out}", str(self.source_path)],
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(m16c_out.read_text(), m30626_out.read_text())

    def test_m30626fjpfp_defines_its_own_macro_in_addition_to_m16c(self):
        # __M16C__ identifies the shared core; __M30626FJPFP__ identifies
        # the specific chip, following the cortex-m4/cortex-m7-over-
        # cortex-m3 pattern of defining both the base and specific macro.
        out = BUILD / "macro_check.s16c"
        result = subprocess.run(
            [str(PCC), "--target=m30626fjpfp", "--format=asm", f"--out={out}", str(self.macro_source_path)],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("mov.w 1,r0", out.read_text())

        plain_out = BUILD / "macro_check_m16c.s16c"
        result = subprocess.run(
            [str(PCC), "--target=m16c", "--format=asm", f"--out={plain_out}", str(self.macro_source_path)],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("mov.w 0,r0", plain_out.read_text())


if __name__ == "__main__":
    unittest.main()
