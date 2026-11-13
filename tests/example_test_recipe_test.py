"""Regression tests for each examples/*/Makefile's `test` recipe.

Run: python3 tests/example_test_recipe_test.py

A `test` recipe that invokes `python3 "$(EXAMPLE_DIR)test.py" ...` hands
python3 an absolute path built from GNU Make's own $(abspath) - which on
Windows comes out as a drive-letter path like "D:/a/rcc/rcc/examples/
hosted-hello/test.py". MSYS2's unprefixed `python` package (the one each
example's test rule actually runs under in CI's `msys2 {0}` shell) has
POSIX path semantics, where a drive-letter path isn't absolute, so it
joins that path onto its own (POSIX-style) cwd and then fails to open
the resulting nonsense path. Every example's `test` rule is always
invoked via `make -C <example dir>` (see .github/workflows/
platformtests.yml), so the recipe never needs an absolute path to find
its own test.py - a plain relative "test.py" works under any python3,
on any OS.
"""

import glob
import os
import re
import subprocess
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EXAMPLES_WITH_TEST_PY = [
    "examples/hosted-hello",
    "examples/z80-coleco",
    "examples/z80-trs80",
    "examples/pdp7-freestanding",
    "examples/6502-dos33",
    "examples/68000-freestanding",
    "examples/6502-freestanding",
]


class ExampleTestRecipeTest(unittest.TestCase):
    def test_no_example_makefile_passes_an_absolute_script_path(self):
        for makefile in glob.glob(os.path.join(ROOT, "examples/*/Makefile")):
            with open(makefile) as f:
                text = f.read()
            self.assertNotRegex(
                text, r'python3\s+"\$\(EXAMPLE_DIR\)test\.py"',
                f"{makefile} passes an absolute $(EXAMPLE_DIR)-based path "
                "to python3 instead of a relative one")

    def test_dry_run_recipe_uses_relative_test_py(self):
        for example in EXAMPLES_WITH_TEST_PY:
            out = subprocess.run(
                ["make", "-C", example, "-n", "test"],
                cwd=ROOT, capture_output=True, text=True, check=True).stdout
            line = next(l for l in out.splitlines() if "python3" in l)
            match = re.search(r'python3\s+"([^"]*)"', line)
            self.assertIsNotNone(match, f"no python3 invocation found for {example}: {line!r}")
            self.assertEqual(match.group(1), "test.py",
                              f"{example}'s test recipe passes {match.group(1)!r} "
                              "instead of a relative 'test.py'")


if __name__ == "__main__":
    unittest.main()
