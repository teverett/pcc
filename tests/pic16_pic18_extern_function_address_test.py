from pathlib import Path
import struct
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
RCC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else ROOT / "build/bin/rcc").resolve()
BUILD = ROOT / "build/pic16-pic18-extern-function-address-tests"

# fa/fb are only ever defined in B_SOURCE, taken as array-initializer
# elements (the dispatch-table idiom) in A_SOURCE and resolved for real at
# --link time - the movlwcl/movlwch cross-file relocation this test covers.
# main() never calls through the table: PIC16 can't call through any
# function pointer at all (a separate, already-fixed restriction), and
# PIC18's CallPtr return-address label has its own pre-existing, unrelated
# separate-compilation gap - this test is scoped to the address-taking
# relocation alone.
A_SOURCE = """
extern int fa();
extern int fb();
int table[2] = {fa, fb};
int main() {
    return 0;
}
"""
B_SOURCE = """
int fa() { return 10; }
int fb() { return 50; }
"""

# A function's address taken from within the SAME file it's defined in,
# under separate compilation - still unsupported (see PIC16Assembler.cpp/
# PIC18Assembler.cpp's movlwcl/movlwch case): resolving it would need the
# field's own non-zero pre-link byte plus this object's base to add
# correctly across the independent lo/hi instruction words.
SAME_FILE_SOURCE = """
int f() { return 1; }
int table[1] = {f};
int main() { return 0; }
"""


def readCoffText(path):
    data = path.read_bytes()
    _, nsec, _, _, _, _, opthdr = struct.unpack_from("<HHIIIHH", data, 0)
    secOff = 20 + opthdr
    for _ in range(nsec):
        name = data[secOff:secOff + 8].rstrip(b"\x00").decode()
        _, _, size, scnptr = struct.unpack_from("<IIII", data, secOff + 8)
        if name == ".text":
            return data[scnptr:scnptr + size]
        secOff += 40
    raise AssertionError(".text section not found")


class PIC16PIC18ExternFunctionAddressTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.a_path = BUILD / "a.c"
        cls.b_path = BUILD / "b.c"
        cls.same_file_path = BUILD / "same_file.c"
        cls.a_path.write_text(A_SOURCE)
        cls.b_path.write_text(B_SOURCE)
        cls.same_file_path.write_text(SAME_FILE_SOURCE)

    def compileAndLink(self, target):
        a_obj = BUILD / f"a_{target}.o"
        b_obj = BUILD / f"b_{target}.o"
        out = BUILD / f"out_{target}.bin"
        for src, obj in ((self.a_path, a_obj), (self.b_path, b_obj)):
            result = subprocess.run(
                [str(RCC), f"--target={target}", "--format=coff", f"--out={obj}", str(src)],
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
        result = subprocess.run(
            [str(RCC), "--link", f"--target={target}", "--format=flat", f"--out={out}", str(a_obj), str(b_obj)],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return a_obj, out

    def test_pic16_compiles_and_links_extern_function_address_in_dispatch_table(self):
        self.compileAndLink("pic16")

    def test_pic18_compiles_and_links_extern_function_address_in_dispatch_table(self):
        self.compileAndLink("pic18")

    def test_pic18_dispatch_table_is_patched_to_fa_and_fb_s_real_linked_address(self):
        # Verifies the relocation produced the exact byte-level value, not
        # just "compiled without crashing": before linking, every
        # movlwcl/movlwch site touching fa/fb is zero-filled (0x0E00 | 0);
        # after linking, two of them hold fn_fa's and fn_fb's real byte
        # addresses, computed independently from each object's own size
        # (fn_fa is the first function in b.c, so its address is exactly
        # a.o's size past where b.o starts; fn_fb follows fn_fa's body).
        a_obj, out_path = self.compileAndLink("pic18")
        a_text = readCoffText(a_obj)
        out = out_path.read_bytes()
        a_base = 0x12  # PIC18LinkerFactory.cpp's fixed kEntryAsm: 9 words

        sites = [i for i in range(0, len(a_text) - 1, 2)
                 if (a_text[i] | (a_text[i + 1] << 8)) & 0xFF00 == 0x0E00
                 and (a_text[i] | (a_text[i + 1] << 8)) & 0xFF == 0]
        patched = [a_base + i for i in sites
                   if (out[a_base + i] | (out[a_base + i + 1] << 8)) & 0xFF != 0]
        self.assertEqual(len(patched), 2, "expected exactly fa's and fb's low-byte sites to be patched")

        fa_addr = a_base + len(a_text)  # fn_fa is the first function in b.o
        b_size = None
        # fn_fb immediately follows fn_fa; both functions are identically
        # shaped ("return <const>;"), so fb's address is fa's plus half of
        # b.o's own .text size.
        with open(BUILD / "b_pic18.o", "rb") as f:
            b_data = f.read()
        _, nsec, _, _, _, _, opthdr = struct.unpack_from("<HHIIIHH", b_data, 0)
        secOff = 20 + opthdr
        for _ in range(nsec):
            name = b_data[secOff:secOff + 8].rstrip(b"\x00").decode()
            _, _, size, _ = struct.unpack_from("<IIII", b_data, secOff + 8)
            if name == ".text":
                b_size = size
            secOff += 40
        fb_addr = fa_addr + b_size // 2

        patchedLowBytes = sorted(out[p] for p in patched)
        self.assertEqual(patchedLowBytes, sorted([fa_addr & 0xFF, fb_addr & 0xFF]))

    def test_pic16_still_rejects_address_of_function_defined_in_same_file(self):
        result = subprocess.run(
            [str(RCC), "--target=pic16", "--format=coff", f"--out={BUILD / 'same16.o'}", str(self.same_file_path)],
            capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("defined in this same file", result.stderr)

    def test_pic18_still_rejects_address_of_function_defined_in_same_file(self):
        result = subprocess.run(
            [str(RCC), "--target=pic18", "--format=coff", f"--out={BUILD / 'same18.o'}", str(self.same_file_path)],
            capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("defined in this same file", result.stderr)


if __name__ == "__main__":
    unittest.main()
