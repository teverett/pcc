from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
PCC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else ROOT / "build/bin/pcc").resolve()
BUILD = ROOT / "build/pic16-function-pointer-tests"

SOURCE = """
int f() { return 42; }
int main() {
    int fp;
    fp = &f;
    return (fp)();
}
"""


class PIC16FunctionPointerCallTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.source_path = BUILD / "fp_call.c"
        cls.source_path.write_text(SOURCE)

    def test_pic16_rejects_call_through_function_pointer(self):
        # No indirect CALL/GOTO exists on the PIC16 core; before this fix,
        # genExpr's switch had no ir::ExprKind::CallPtr case, so this call
        # was silently dropped instead of being rejected.
        result = subprocess.run(
            [str(PCC), "--target=pic16", "--format=asm", str(self.source_path)],
            capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("function pointer", result.stderr)

    def test_pic18_still_supports_call_through_function_pointer(self):
        # PIC18 has a real indirect-call implementation (PCLATU/PCLATH/PCL
        # loaded from the callee address) - confirms the PIC16 fix didn't
        # regress the sibling backend that already supports this.
        output_path = BUILD / "fp_call_pic18.s"
        result = subprocess.run(
            [str(PCC), "--target=pic18", "--format=asm", f"--out={output_path}", str(self.source_path)],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("PCLATU", output_path.read_text())


if __name__ == "__main__":
    unittest.main()
