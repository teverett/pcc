from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
RCC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else ROOT / "build/bin/rcc").resolve()
BUILD = ROOT / "build/platform-flag-tests"


# --platform=<name> is pure sugar over --target/--format, and
# --list-platforms enumerates every <target>-<host> triple and every
# --platform name this build accepts - both generated from the same
# allPlatforms()/kTargetNames tables in src/main.cpp, never hand-
# maintained, so they can't drift from what --target/--format/--platform
# actually support. --list-triples (the narrower, now-removed flag that
# used to show only <target>-<host> triples) has been folded into this
# one combined --list-platforms and no longer exists.
class PlatformFlagTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)

    def run_rcc(self, *args):
        return subprocess.run([str(RCC), *[str(a) for a in args]], capture_output=True, text=True)

    def compile_to_coff(self, stem, target, source):
        source_path = BUILD / f"{stem}.c"
        object_path = BUILD / f"{stem}.o"
        source_path.write_text(source)
        result = self.run_rcc(f"--target={target}", "--format=coff", f"--out={object_path}", source_path)
        self.assertEqual(result.returncode, 0, result.stderr)
        return object_path

    def test_list_platforms_contains_expected_names_including_formatless_targets(self):
        names = set(self.run_rcc("--list-platforms").stdout.splitlines())
        for expected in (
            "6502-dos33", "65c02-prodos", "z80-coleco-colecorom",
            "6809-decb", "8086-dos-raw", "saturn-coff", "i386-linux",
            # pdp1 has no --link step at all, and tms320c6713's own
            # TargetLinker::outputFormats() is empty (asm-only) - both fall
            # back to their single-file-compile Backend::assembledFormats()
            # instead of being silently absent from this table.
            "pdp1-list", "pdp1-rim", "tms320c6713-raw",
        ):
            self.assertIn(expected, names)
        # "asm" means "print the assembly text", not a file-produced
        # convention - it must never appear as a platform's format suffix.
        self.assertFalse(any(name.endswith("-asm") for name in names))

    def test_list_triples_no_longer_exists(self):
        result = self.run_rcc("--list-triples")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unknown option", result.stderr)

    def test_platform_resolves_identically_to_explicit_target_and_format(self):
        # One target with no dash of its own, one whose target name itself
        # contains a dash (z80-coleco) - proving resolution never re-splits
        # the generated name, only looks it up against the known table.
        cases = [
            ("6502", "dos33", "6502-dos33"),
            ("z80-coleco", "colecorom", "z80-coleco-colecorom"),
        ]
        for target, fmt, platform in cases:
            obj = self.compile_to_coff(f"src_{platform}", target, "int main() { return 42; }\n")
            explicit_out = BUILD / f"explicit_{platform}"
            platform_out = BUILD / f"platform_{platform}"
            explicit = self.run_rcc("--link", f"--target={target}", f"--format={fmt}", f"--out={explicit_out}", obj)
            self.assertEqual(explicit.returncode, 0, explicit.stderr)
            via_platform = self.run_rcc("--link", f"--platform={platform}", f"--out={platform_out}", obj)
            self.assertEqual(via_platform.returncode, 0, via_platform.stderr)
            self.assertEqual(explicit_out.read_bytes(), platform_out.read_bytes())

    def test_unknown_platform_name_fails_clearly(self):
        result = self.run_rcc("--platform=bogus-name", "--out=" + str(BUILD / "x"), BUILD / "nonexistent.c")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("--list-platforms", result.stderr)

    def test_platform_combined_with_target_is_rejected_explicitly(self):
        result = self.run_rcc(
            "--platform=6502-dos33", "--target=6502", "--out=" + str(BUILD / "x"), BUILD / "nonexistent.c",
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("--platform", result.stderr)
        self.assertIn("--target", result.stderr)


if __name__ == "__main__":
    unittest.main()
