from pathlib import Path
import os
import struct
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
RCC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else ROOT / "build/bin/rcc").resolve()
BUILD = ROOT / "build/hosted-linux-unit-tests"


def shared_library(path, symbols, soname="libversions.so", versions=()):
    strings = bytearray(b"\0")

    def string(value):
        at = len(strings)
        strings.extend(value.encode() + b"\0")
        return at

    names = [string(name) for name, _, _ in symbols]
    version_names = [string(name) for name in versions]
    library_name = string(soname)
    dynsym = bytearray(16)
    versym = bytearray(2)
    for (name, kind, version), offset in zip(symbols, names):
        dynsym.extend(struct.pack("<IIIBBH", offset, 0x1000, 1, 0x10 | kind, 0, 1))
        versym.extend(struct.pack("<H", version))
    definitions = bytearray()
    for index, offset in enumerate(version_names):
        definitions.extend(struct.pack("<HHHHIII", 1, 0, index + 2, 1, 0, 20,
                                       28 if index + 1 < len(versions) else 0))
        definitions.extend(struct.pack("<II", offset, 0))
    sections = [
        (0, b"", 0, 0, 0),
        (3, strings, 0, 0, 0),
        (11, dynsym, 1, 0, 16),
        (0x6fffffff, versym, 2, 0, 2),
        (0x6ffffffd, definitions, 1, len(versions), 0),
        (6, struct.pack("<IIII", 14, library_name, 0, 0), 1, 0, 8),
    ]
    data = bytearray(52 + len(sections) * 40)
    data[:7] = b"\x7fELF\x01\x01\x01"
    struct.pack_into("<HHI", data, 16, 3, 3, 1)
    struct.pack_into("<I", data, 32, 52)
    struct.pack_into("<HHHH", data, 40, 52, 0, 0, 40)
    struct.pack_into("<H", data, 48, len(sections))
    for index, (kind, content, link, info, entry) in enumerate(sections):
        at = 52 + index * 40
        struct.pack_into("<IIIIIIIIII", data, at, 0, kind, 0, 0, len(data),
                         len(content), link, info, 1, entry)
        data.extend(content)
    path.write_bytes(data)


def dynamic_info(path):
    data = path.read_bytes()
    phoff = struct.unpack_from("<I", data, 28)[0]
    size, count = struct.unpack_from("<HH", data, 42)
    headers = [struct.unpack_from("<IIIIIIII", data, phoff + index * size)
               for index in range(count)]
    header = next(item for item in headers if item[0] == 2)
    tags = {}
    for at in range(header[1], header[1] + header[4], 8):
        tag, value = struct.unpack_from("<II", data, at)
        if not tag:
            break
        tags.setdefault(tag, []).append(value)
    strings = data[tags[5][0] - 0x08048000:][:tags[10][0]]
    needed = [strings[offset:].split(b"\0", 1)[0].decode()
              for offset in tags.get(1, [])]
    return data, tags, strings, needed


class LinuxHostedLinkerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)

    def compile(self, stem, source):
        src, obj = BUILD / (stem + ".c"), BUILD / (stem + ".o")
        src.write_text(source)
        result = subprocess.run([str(RCC), "--target=i386-linux",
                                 f"--out={obj}", str(src)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return obj

    def link(self, stem, obj, *args):
        output = BUILD / stem
        output.write_bytes(b"preserved output")
        result = subprocess.run([str(RCC), "--link", "--target=i386-linux",
                                 f"--out={output}", str(obj), *map(str, args)],
                                capture_output=True, text=True)
        return result, output

    def fixture(self, stem, symbols=(("library_value", 2, 1),), versions=()):
        library = BUILD / (stem + ".so")
        shared_library(library, symbols, versions=versions)
        return library

    def archive(self, stem, *objects):
        data = bytearray(b"!<arch>\n")
        for index, obj in enumerate(objects):
            content = obj.read_bytes()
            header = (f"member{index}.o/".ljust(16) + "0".ljust(12) + "0".ljust(6) +
                      "0".ljust(6) + "100644".ljust(8) + str(len(content)).ljust(10) + "`\n")
            data.extend(header.encode() + content + (b"\n" if len(content) & 1 else b""))
        archive = BUILD / (stem + ".a")
        archive.write_bytes(data)
        return archive

    def test_group_rescans_previous_archive(self):
        main = self.compile("rescan-main", "int first(); int main(){return first();}")
        first = self.compile("rescan-first", "int second(); int first(){return second();}")
        second = self.compile("rescan-second", "int second(){return 42;}")
        a = self.archive("rescan-a", first)
        b = self.archive("rescan-b", second)
        script = BUILD / "rescan.so"
        script.write_text(f"GROUP ( {b} {a} )")
        result, output = self.link("rescan-app", main, script)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output.read_bytes()[:4], b"\x7fELF")

    def test_group_resolves_circular_archive_dependency(self):
        main = self.compile("circular-main", "int first(); int main(){return first();}")
        first = self.compile("circular-first", "int second(); int first(){return second();}")
        second = self.compile("circular-second", "int third(); int second(){return third();}")
        third = self.compile("circular-third", "int third(){return 37;}")
        a = self.archive("circular-a", first, third)
        b = self.archive("circular-b", second)
        script = BUILD / "circular.so"
        script.write_text(f"GROUP ( {a} {b} )")
        result, output = self.link("circular-app", main, script)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output.read_bytes()[:4], b"\x7fELF")

    def test_input_script_and_soname(self):
        obj = self.compile("input-main", "int library_value(); int main(){return library_value();}")
        library = self.fixture("input-lib")
        script = BUILD / "input.so"
        script.write_text(f'/* library */ OUTPUT_FORMAT(elf32-i386)\nINPUT("{library}")')
        result, output = self.link("input-app", obj, script)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(dynamic_info(output)[3], ["libversions.so"])

    def test_group_nested_as_needed_excludes_unused_library(self):
        obj = self.compile("group-main", "int library_value(); int main(){return library_value();}")
        library = self.fixture("group-lib")
        unused = self.fixture("unused-lib", (("unused_function", 2, 1),))
        shared_library(unused, (("unused_function", 2, 1),), soname="libunused.so")
        script = BUILD / "group.so"
        script.write_text(f"GROUP ( AS_NEEDED ( {unused} ) INPUT ( {library} ) )")
        result, output = self.link("group-app", obj, script)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(dynamic_info(output)[3], ["libversions.so"])

    def test_as_needed_retains_used_library(self):
        obj = self.compile("needed-main", "int library_value(); int main(){return library_value();}")
        library = self.fixture("needed-lib")
        script = BUILD / "needed.so"
        script.write_text(f"GROUP ( AS_NEEDED ( {library} ) )")
        result, output = self.link("needed-app", obj, script)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(dynamic_info(output)[3], ["libversions.so"])

    def test_script_cycle_preserves_output(self):
        obj = self.compile("cycle-main", "int main(){return 0;}")
        script = BUILD / "cycle.so"
        script.write_text(f"INPUT ( {script} )")
        result, output = self.link("cycle-app", obj, script)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("cyclic GNU", result.stderr)
        self.assertEqual(output.read_bytes(), b"preserved output")

    def test_unsupported_script_directive_preserves_output(self):
        obj = self.compile("directive-main", "int main(){return 0;}")
        script = BUILD / "directive.so"
        script.write_text("SECTIONS { .text : { *(.text) } }")
        result, output = self.link("directive-app", obj, script)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported GNU linker script directive", result.stderr)
        self.assertEqual(output.read_bytes(), b"preserved output")

    def test_oversized_script_preserves_output(self):
        obj = self.compile("oversized-main", "int main(){return 0;}")
        script = BUILD / "oversized.so"
        script.write_text(" " * 65537)
        result, output = self.link("oversized-app", obj, script)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("64 KiB limit", result.stderr)
        self.assertEqual(output.read_bytes(), b"preserved output")

    def test_script_nesting_limit_preserves_output(self):
        obj = self.compile("nesting-main", "int main(){return 0;}")
        script = BUILD / "nesting.so"
        script.write_text("GROUP (" + "AS_NEEDED (" * 10 + str(self.fixture("nesting-lib")) +
                          ")" * 11)
        result, output = self.link("nesting-app", obj, script)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("nesting limit", result.stderr)
        self.assertEqual(output.read_bytes(), b"preserved output")

    def test_default_symbol_version_requirement(self):
        obj = self.compile("version-main", "int library_value(); int main(){return library_value();}")
        library = self.fixture("version-lib", (("library_value", 2, 2),), ("RCC_1.0",))
        result, output = self.link("version-app", obj, library)
        self.assertEqual(result.returncode, 0, result.stderr)
        data, tags, strings, _ = dynamic_info(output)
        self.assertEqual(tags[0x6fffffff], [1])
        need = tags[0x6ffffffe][0] - 0x08048000
        version_name = struct.unpack_from("<I", data, need + 24)[0]
        self.assertEqual(strings[version_name:].split(b"\0", 1)[0], b"RCC_1.0")
        symbol_versions = tags[0x6ffffff0][0] - 0x08048000
        self.assertEqual(struct.unpack_from("<HH", data, symbol_versions), (0, 2))

    def test_hidden_compatibility_version_does_not_replace_default(self):
        obj = self.compile("default-main", "int library_value(); int main(){return library_value();}")
        library = self.fixture("default-lib", (("library_value", 2, 3),
                                                ("library_value", 2, 0x8002)),
                               ("RCC_OLD", "RCC_NEW"))
        result, output = self.link("default-app", obj, library)
        self.assertEqual(result.returncode, 0, result.stderr)
        data, tags, strings, _ = dynamic_info(output)
        need = tags[0x6ffffffe][0] - 0x08048000
        version_name = struct.unpack_from("<I", data, need + 24)[0]
        self.assertEqual(strings[version_name:].split(b"\0", 1)[0], b"RCC_NEW")

    def test_single_import_plt0_is_exactly_sixteen_bytes(self):
        obj = self.compile("plt-main", "int library_value(); int main(){return library_value();}")
        result, output = self.link("plt-app", obj, self.fixture("plt-lib"))
        self.assertEqual(result.returncode, 0, result.stderr)
        data, tags, _, _ = dynamic_info(output)
        got = tags[3][0] - 0x08048000
        self.assertEqual(struct.unpack_from("<IIIII", data, tags[4][0] - 0x08048000),
                         (1, 2, 1, 0, 0))
        self.assertEqual(data[got - 20:got - 16], b"\x0f\x1f\x40\0")
        self.assertEqual(data[got - 16:got - 14], b"\xff\x25")
        self.assertEqual(struct.unpack_from("<I", data, got - 9)[0], 0)
        startup_call = data.index(b"\xe8", 4096)
        imported_call = data.index(b"\xe8", startup_call + 5)
        displacement = struct.unpack_from("<i", data, imported_call + 1)[0]
        self.assertEqual(imported_call + 5 + displacement, got - 16)

    def test_multiple_import_plt_resolver_uses_byte_offsets(self):
        obj = self.compile("multi-main",
                           "int first(); int second(); int main(){return first()+second();}")
        library = self.fixture("multi-lib", (("first", 2, 1), ("second", 2, 1)))
        result, output = self.link("multi-app", obj, library)
        self.assertEqual(result.returncode, 0, result.stderr)
        data, tags, _, _ = dynamic_info(output)
        self.assertEqual(tags[2], [16])
        got = tags[3][0] - 0x08048000
        self.assertEqual(struct.unpack_from("<IIIIII", data, tags[4][0] - 0x08048000),
                         (1, 3, 1, 0, 2, 0))
        self.assertEqual(struct.unpack_from("<I", data, got - 25)[0], 0)
        self.assertEqual(struct.unpack_from("<I", data, got - 9)[0], 8)
        startup_call = data.index(b"\xe8", 4096)
        first_call = data.index(b"\xe8", startup_call + 5)
        second_call = data.index(b"\xe8", first_call + 5)
        targets = [call + 5 + struct.unpack_from("<i", data, call + 1)[0]
                   for call in (first_call, second_call)]
        self.assertEqual(sorted(targets), [got - 32, got - 16])
        rel = tags[23][0] - 0x08048000
        self.assertEqual(struct.unpack_from("<IIII", data, rel),
                         (0x08048000 + got + 12, 0x107, 0x08048000 + got + 16, 0x207))

    def test_data_import_is_explicitly_rejected(self):
        obj = self.compile("data-main", "int library_value(); int main(){return library_value();}")
        result, output = self.link("data-app", obj, self.fixture("data-lib", (("library_value", 1, 1),)))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported ELF data/TLS import", result.stderr)
        self.assertEqual(output.read_bytes(), b"preserved output")

    def test_untyped_direct_function_import(self):
        obj = self.compile("untyped-main", "int library_value(); int main(){return library_value();}")
        result, output = self.link("untyped-app", obj,
                                   self.fixture("untyped-lib", (("library_value", 0, 1),)))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(dynamic_info(output)[1][2], [8])

    def test_two_untyped_direct_function_imports(self):
        obj = self.compile("untyped-multi-main",
                           "int first(); int second(); int main(){return first()+second();}")
        result, output = self.link("untyped-multi-app", obj,
                                   self.fixture("untyped-multi-lib", (("first", 0, 1), ("second", 0, 1))))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(dynamic_info(output)[1][2], [16])

    def test_tls_import_is_explicitly_rejected(self):
        obj = self.compile("tls-main", "int library_value(); int main(){return library_value();}")
        result, output = self.link("tls-app", obj, self.fixture("tls-lib", (("library_value", 6, 1),)))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported ELF data/TLS import", result.stderr)
        self.assertEqual(output.read_bytes(), b"preserved output")

    def test_native_initialized_data_section_is_explicitly_rejected(self):
        obj = self.compile("initialized-data-main", "int main(){return 37;}")
        data = bytearray(obj.read_bytes())
        shoff = struct.unpack_from("<I", data, 32)[0]
        section = shoff + 2 * 40
        struct.pack_into("<II", data, section + 4, 1, 3)
        struct.pack_into("<II", data, section + 16, len(data), 4)
        data.extend(struct.pack("<I", 42))
        obj.write_bytes(data)
        result, output = self.link("initialized-data-app", obj)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported allocated ELF object section", result.stderr)
        self.assertEqual(output.read_bytes(), b"preserved output")

    def test_native_multiple_text_sections_are_explicitly_rejected(self):
        obj = self.compile("multiple-text-main", "int main(){return 37;}")
        data = bytearray(obj.read_bytes())
        shoff = struct.unpack_from("<I", data, 32)[0]
        section = shoff + 2 * 40
        struct.pack_into("<II", data, section + 4, 1, 6)
        struct.pack_into("<II", data, section + 16, len(data), 4)
        data.extend(b"\x90\x90\x90\xc3")
        obj.write_bytes(data)
        result, output = self.link("multiple-text-app", obj)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported allocated ELF object section", result.stderr)
        self.assertEqual(output.read_bytes(), b"preserved output")

    def libc_paths(self):
        configured = os.environ.get("RCC_LINUX_LIBRARY_PATHS")
        paths = configured.split(os.pathsep) if configured else [
            "/usr/lib/i386-linux-gnu", "/lib/i386-linux-gnu", "/usr/lib32", "/lib32"]
        if not any((Path(path) / "libc.so").exists() for path in paths):
            self.skipTest("i386 libc development files not installed")
        return [f"-L{path}" for path in paths]

    def test_real_libc_startup_and_puts_versions(self):
        paths = self.libc_paths()
        obj = self.compile("libc-main", 'int puts(char *text); int main(){puts("hello rcc");return 0;}')
        result, output = self.link("libc-app", obj, *paths, "-lc")
        self.assertEqual(result.returncode, 0, result.stderr)
        data, tags, strings, needed = dynamic_info(output)
        self.assertIn("libc.so.6", needed)
        self.assertNotIn("ld-linux.so.2", needed)
        self.assertIn(b"__libc_start_main\0", strings)
        self.assertIn(b"puts\0", strings)
        self.assertIn(b"GLIBC_", strings)
        self.assertEqual(data[4096:4114],
                         bytes.fromhex("31ed5e89e183e4f031c05054525050515668"))
        self.assertIn(0x6ffffff0, tags)

    def test_real_libc_initialization_even_without_user_imports(self):
        paths = self.libc_paths()
        obj = self.compile("libc-status-main", "int main(){return 37;}")
        result, output = self.link("libc-status-app", obj, *paths, "-l", "c")
        self.assertEqual(result.returncode, 0, result.stderr)
        data, tags, strings, needed = dynamic_info(output)
        self.assertIn("libc.so.6", needed)
        self.assertIn(b"__libc_start_main\0", strings)
        self.assertEqual(tags[2], [8])
        self.assertEqual(data[4108], 0x52)


if __name__ == "__main__":
    unittest.main()
