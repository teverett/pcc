from pathlib import Path
import re
import struct
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
PCC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else ROOT / "build/bin/pcc").resolve()
BUILD = ROOT / "build/hosted-internal-linker-tests"


class HostedInternalLinkerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)

    def compile(self, target, hosted, stem, source):
        source_path = BUILD / f"{stem}.c"
        object_path = BUILD / f"{stem}.o"
        source_path.write_text(source)
        result = subprocess.run(
            [str(PCC), f"--target={target}", hosted, "--format=" +
             ("coff" if hosted == "--hosted=windows" else "macho" if hosted == "--hosted=macos" else "elf"),
             f"--out={object_path}", str(source_path)],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return object_path

    def link(self, target, hosted, object_path, output_path, *extra):
        fmt = "coff" if hosted == "--hosted=windows" else "macho" if hosted == "--hosted=macos" else "elf"
        result = subprocess.run(
            [str(PCC), "--link", f"--target={target}", hosted, f"--format={fmt}",
             f"--out={output_path}", str(object_path), *extra],
            capture_output=True, text=True,
        )
        return result

    def test_linux_emits_internal_i386_executable(self):
        obj = self.compile("i386", "--hosted", "elf-main", "int main() { return 37; }\n")
        output = BUILD / "elf-app"
        result = self.link("i386", "--hosted", obj, output)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = output.read_bytes()
        self.assertEqual(data[:7], b"\x7fELF\x01\x01\x01")
        self.assertEqual(struct.unpack_from("<HH", data, 16), (2, 3))
        entry = struct.unpack_from("<I", data, 24)[0]
        phoff = struct.unpack_from("<I", data, 28)[0]
        phentsize, phnum = struct.unpack_from("<HH", data, 42)
        self.assertGreater(entry, 0x08048000)
        self.assertEqual(phnum, 2)
        load = struct.unpack_from("<IIIIIIII", data, phoff)
        self.assertEqual(load[0], 1)
        self.assertEqual(load[6], 7)
        self.assertLessEqual(load[2], entry)
        self.assertGreaterEqual(load[2] + load[5], entry)

    def test_windows_emits_pe_with_kernel32_import_and_exit_thunk(self):
        obj = self.compile("i386", "--hosted=windows", "pe-main", "int main() { return 37; }\n")
        output = BUILD / "pe-app.exe"
        result = self.link("i386", "--hosted=windows", obj, output)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = output.read_bytes()
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        self.assertEqual(data[:2], b"MZ")
        self.assertEqual(data[pe:pe + 4], b"PE\0\0")
        self.assertEqual(struct.unpack_from("<H", data, pe + 4)[0], 0x14c)
        optional = pe + 24
        self.assertEqual(struct.unpack_from("<H", data, optional)[0], 0x10b)
        import_rva, import_size = struct.unpack_from("<II", data, optional + 104)
        iat_rva, iat_size = struct.unpack_from("<II", data, optional + 96 + 8 * 12)
        section = optional + struct.unpack_from("<H", data, pe + 20)[0]
        virtual_address = struct.unpack_from("<I", data, section + 12)[0]
        raw_offset = struct.unpack_from("<I", data, section + 20)[0]

        def file_offset(rva):
            return raw_offset + rva - virtual_address

        self.assertEqual(import_size, 40)
        self.assertEqual(iat_size, 8)
        descriptor = file_offset(import_rva)
        original_thunk, _, _, name_rva, first_thunk = struct.unpack_from("<IIIII", data, descriptor)
        self.assertEqual(first_thunk, iat_rva)
        self.assertEqual(original_thunk, iat_rva - 8)
        name = data[file_offset(name_rva):].split(b"\0", 1)[0]
        self.assertEqual(name.lower(), b"kernel32.dll")
        hint_name = struct.unpack_from("<I", data, file_offset(iat_rva))[0]
        self.assertEqual(data[file_offset(hint_name) + 2:].split(b"\0", 1)[0], b"ExitProcess")
        self.assertIn(b"\xff\x25", data[raw_offset:raw_offset + struct.unpack_from("<I", data, section + 16)[0]])

    def test_windows_lazily_imports_function_from_coff_import_library(self):
        obj = self.compile("i386", "--hosted=windows", "import-main",
                           "extern int imported(); int main() { return imported(); }\n")
        symbol = b"_imported"
        dll = b"sample.dll"
        member = struct.pack("<HHHHIIHH", 0, 0xffff, 0, 0x14c, 0,
                             len(symbol) + len(dll) + 2, 0, 2 << 2)
        member += symbol + b"\0" + dll + b"\0"

        def archive_member(name, content):
            fields = [
                (name + "/").ljust(16), "0".ljust(12), "0".ljust(6), "0".ljust(6),
                "100644".ljust(8), str(len(content)).ljust(10), "`\n",
            ]
            header = "".join(fields).encode("ascii")
            self.assertEqual(len(header), 60)
            return header + content + (b"\n" if len(content) & 1 else b"")

        library = BUILD / "custom.lib"
        library.write_bytes(b"!<arch>\n" + archive_member("import.obj", member))
        output = BUILD / "pe-import-library.exe"
        result = self.link("i386", "--hosted=windows", obj, output, f"-L{BUILD}", "-lcustom")
        self.assertEqual(result.returncode, 0, result.stderr)
        data = output.read_bytes()
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        optional = pe + 24
        import_rva, import_size = struct.unpack_from("<II", data, optional + 104)
        section = optional + struct.unpack_from("<H", data, pe + 20)[0]
        virtual_address = struct.unpack_from("<I", data, section + 12)[0]
        raw_offset = struct.unpack_from("<I", data, section + 20)[0]

        def file_offset(rva):
            return raw_offset + rva - virtual_address

        libraries = []
        descriptor = file_offset(import_rva)
        end = descriptor + import_size
        while descriptor + 20 <= end:
            original_thunk, _, _, name_rva, _ = struct.unpack_from("<IIIII", data, descriptor)
            if not (original_thunk or name_rva):
                break
            libraries.append(data[file_offset(name_rva):].split(b"\0", 1)[0].lower())
            descriptor += 20
        self.assertIn(b"sample.dll", libraries)
        self.assertIn(b"kernel32.dll", libraries)

    def test_external_linker_selection_fails_without_replacing_output(self):
        obj = self.compile("i386", "--hosted", "unsupported-main", "int main() { return 0; }\n")
        output = BUILD / "preserved-output"
        output.write_bytes(b"keep this file")
        result = subprocess.run(
            [str(PCC), "--link", "--target=i386", "--hosted", "--format=elf",
             f"--linker={BUILD / 'must-not-run'}", f"--out={output}", str(obj)],
            capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("hosted linking is internal", result.stderr)
        self.assertEqual(output.read_bytes(), b"keep this file")

    def pe_import_fixture(self, stem, type_info, symbol=b"_imported", ordinal=0, source=None):
        obj = self.compile("i386", "--hosted=windows", stem,
                           source or "extern int imported(); int main() { return imported(); }\n")
        dll = b"sample.dll"
        payload = symbol + b"\0" + dll + b"\0"
        record = struct.pack("<HHHHIIHH", 0, 0xffff, 0, 0x14c, 0, len(payload), ordinal, type_info) + payload
        header = ("import.obj/".ljust(16) + "0".ljust(12) + "0".ljust(6) +
                  "0".ljust(6) + "100644".ljust(8) + str(len(record)).ljust(10) + "`\n").encode()
        library = BUILD / f"{stem}.lib"
        library.write_bytes(b"!<arch>\n" + header + record + (b"\n" if len(record) & 1 else b""))
        output = BUILD / f"{stem}.exe"
        result = self.link("i386", "--hosted=windows", obj, output, str(library))
        self.assertEqual(result.returncode, 0, result.stderr)
        data = output.read_bytes()
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        optional = pe + 24
        section = optional + struct.unpack_from("<H", data, pe + 20)[0]
        va = struct.unpack_from("<I", data, section + 12)[0]
        raw = struct.unpack_from("<I", data, section + 20)[0]
        def offset(rva):
            return raw + rva - va
        descriptor = offset(struct.unpack_from("<I", data, optional + 104)[0])
        while True:
            lookup, _, _, name, iat = struct.unpack_from("<IIIII", data, descriptor)
            self.assertNotEqual(name, 0, "sample import descriptor missing")
            if data[offset(name):].split(b"\0", 1)[0] == dll:
                return data, offset, lookup, iat, raw
            descriptor += 20

    def test_windows_import_thunk_targets_iat_slot(self):
        data, offset, lookup, iat, raw = self.pe_import_fixture("iat-target", 2 << 2)
        operand = struct.pack("<I", 0x400000 + iat)
        self.assertIn(b"\xff\x25" + operand, data[raw:])
        hint_name = struct.unpack_from("<I", data, offset(lookup))[0]
        self.assertNotEqual(hint_name, iat)
        self.assertEqual(data[offset(hint_name) + 2:].split(b"\0", 1)[0], b"imported")

    def test_windows_ordinal_import_encodes_high_bit(self):
        data, offset, lookup, iat, _ = self.pe_import_fixture("ordinal-import", 0, ordinal=42)
        self.assertEqual(struct.unpack_from("<I", data, offset(lookup))[0], 0x8000002a)
        self.assertEqual(struct.unpack_from("<I", data, offset(iat))[0], 0x8000002a)

    def test_windows_name_import_preserves_decoration(self):
        data, offset, lookup, _, _ = self.pe_import_fixture("decorated-import", 1 << 2)
        hint_name = struct.unpack_from("<I", data, offset(lookup))[0]
        self.assertEqual(data[offset(hint_name) + 2:].split(b"\0", 1)[0], b"_imported")

    def test_windows_data_import_resolves_import_pointer(self):
        data, _, _, iat, raw = self.pe_import_fixture(
            "data-import", (2 << 2) | 1,
            source="extern int _imp__imported(); int main() { return (int)&_imp__imported; }\n",
        )
        self.assertIn(b"\xb8" + struct.pack("<I", 0x400000 + iat), data[raw:])

    def test_windows_import_pointer_alias_uses_iat_not_thunk(self):
        data, _, _, iat, raw = self.pe_import_fixture(
            "function-pointer-import", 2 << 2,
            source="extern int _imp__imported(); int main() { return (int)&_imp__imported; }\n",
        )
        self.assertIn(b"\xb8" + struct.pack("<I", 0x400000 + iat), data[raw:])

    def dll_fixture(self, filename, machine=0x14c, export="imported"):
        data = bytearray(0x400)
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 0x3c, 0x80)
        data[0x80:0x84] = b"PE\0\0"
        struct.pack_into("<HHIIIHH", data, 0x84, machine, 1, 0, 0, 0, 224, 0x2102)
        optional = 0x98
        struct.pack_into("<H", data, optional, 0x10b)
        struct.pack_into("<I", data, optional + 92, 16)
        struct.pack_into("<II", data, optional + 96, 0x1020, 0x100)
        section = optional + 224
        data[section:section + 8] = b".text\0\0\0"
        struct.pack_into("<IIII", data, section + 8, 0x200, 0x1000, 0x200, 0x200)
        struct.pack_into("<I", data, section + 36, 0x60000020)
        data[0x200:0x206] = b"\xb8\x2a\0\0\0\xc3"
        exports = [export] if isinstance(export, str) else export
        struct.pack_into("<IIIIII", data, 0x220 + 16, 1, len(exports), len(exports), 0x1060, 0x1070, 0x1080)
        at = 0x2a0
        for i, exported in enumerate(exports):
            struct.pack_into("<I", data, 0x260 + i * 4, 0x1000)
            struct.pack_into("<I", data, 0x270 + i * 4, 0x1000 + at - 0x200)
            struct.pack_into("<H", data, 0x280 + i * 2, i)
            name = exported.encode("ascii") + b"\0"
            data[at:at + len(name)] = name
            at += len(name)
        path = BUILD / filename
        path.write_bytes(data)
        return path

    def test_windows_resolves_named_export_from_dll(self):
        obj = self.compile("i386", "--hosted=windows", "dll-main",
                           "extern int imported(); int main() { return imported(); }\n")
        dll = self.dll_fixture("sample-export.dll")
        output = BUILD / "dll-export-app.exe"
        result = self.link("i386", "--hosted=windows", obj, output, str(dll))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"sample-export.dll\0", output.read_bytes())
        self.assertIn(b"imported\0", output.read_bytes())

    def test_windows_rejects_wrong_architecture_dll_without_replacing_output(self):
        obj = self.compile("i386", "--hosted=windows", "dll-invalid-main",
                           "extern int imported(); int main() { return imported(); }\n")
        dll = self.dll_fixture("sample-wrong.dll", machine=0x8664)
        output = BUILD / "preserved-dll-output"
        output.write_bytes(b"keep")
        result = self.link("i386", "--hosted=windows", obj, output, str(dll))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("i386 DLL", result.stderr)
        self.assertEqual(output.read_bytes(), b"keep")

    def test_windows_crt_uses_exit_import_for_main_termination(self):
        main = self.compile("i386", "--hosted=windows", "crt-exit-main", "int main() { return 37; }\n")
        dll = self.dll_fixture("msvcrt.dll", export="exit")
        output = BUILD / "crt-exit.exe"
        result = self.link("i386", "--hosted=windows", main, output, str(dll))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"msvcrt.dll\0", output.read_bytes())
        self.assertIn(b"\0\0exit\0", output.read_bytes())

    def test_windows_crt_library_search_resolves_real_dll(self):
        main = self.compile("i386", "--hosted=windows", "crt-search-main", "int main() { return 37; }\n")
        self.dll_fixture("msvcrt.dll", export="exit")
        output = BUILD / "crt-search.exe"
        result = self.link("i386", "--hosted=windows", main, output, "-L", str(BUILD), "-lmsvcrt")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"msvcrt.dll\0", output.read_bytes())
        self.assertIn(b"\0\0exit\0", output.read_bytes())

    def test_windows_crt_resolves_puts_without_a_builtin_runtime(self):
        main = self.compile("i386", "--hosted=windows", "crt-puts-main",
                            'int puts(char *text); int main() { puts("hello pcc"); return 0; }\n')
        dll = self.dll_fixture("msvcrt.dll", export=["puts", "exit"])
        output = BUILD / "crt-puts.exe"
        result = self.link("i386", "--hosted=windows", main, output, str(dll))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"\0\0puts\0", output.read_bytes())
        self.assertIn(b"\0\0exit\0", output.read_bytes())

    def test_windows_crt_nonzero_main_status_links_puts_and_exit(self):
        main = self.compile("i386", "--hosted=windows", "crt-status-main",
                            'int puts(char *text); int main() { puts("returning status"); return 37; }\n')
        dll = self.dll_fixture("msvcrt.dll", export=["puts", "exit"])
        output = BUILD / "crt-status.exe"
        result = self.link("i386", "--hosted=windows", main, output, str(dll))
        self.assertEqual(result.returncode, 0, result.stderr)
        data = output.read_bytes()
        self.assertIn(b"\0\0puts\0", data)
        self.assertIn(b"\0\0exit\0", data)
        self.assertIn(b"\xb8\x25\0\0\0", data)

    def coff_sections_fixture(self, filename):
        code = b"\xa1\0\0\0\0\x03\x05\0\0\0\0\xc3"
        data = bytearray(20 + 3 * 40)
        text_offset = len(data)
        data += code
        data_offset = len(data)
        data += struct.pack("<I", 17)
        reloc_offset = len(data)
        data += struct.pack("<IIH", 1, 1, 6) + struct.pack("<IIH", 7, 2, 6)
        symbol_offset = len(data)
        for name, section, storage in [(b"_imported", 1, 2), (b"datum", 2, 3), (b"zero", 3, 3)]:
            if len(name) > 8:
                encoded = struct.pack("<II", 0, 4)
            else:
                encoded = name.ljust(8, b"\0")
            data += encoded + struct.pack("<IhHBB", 0, section, 0, storage, 0)
        data += struct.pack("<I", 14) + b"_imported\0"
        struct.pack_into("<HHIIIHH", data, 0, 0x14c, 3, 0, symbol_offset, 3, 0, 0)
        for i, (name, size, raw, reloc, num, flags) in enumerate([
                (b".text", len(code), text_offset, reloc_offset, 2, 0x60500020),
                (b".data", 4, data_offset, 0, 0, 0xc0300040),
                (b".bss", 4, 0, 0, 0, 0xc0300080)]):
            header = 20 + i * 40
            data[header:header + 8] = name.ljust(8, b"\0")
            struct.pack_into("<IIIIIIHHI", data, header + 8, 0, 0, size, raw, reloc, 0, num, 0, flags)
        path = BUILD / filename
        path.write_bytes(data)
        return path

    def pe_addresses(self, data):
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        optional = pe + 24
        section = optional + struct.unpack_from("<H", data, pe + 20)[0]
        entries = []
        for i in range(struct.unpack_from("<H", data, pe + 6)[0]):
            header = section + 40 * i
            size, rva, raw_size, raw = struct.unpack_from("<IIII", data, header + 8)
            entries.append((rva, size, raw_size, raw))

        def file_offset(rva):
            for address, _, size, raw in entries:
                if address <= rva < address + size:
                    return raw + rva - address
            self.fail(f"RVA {rva:x} has no file-backed section")

        rva, size = struct.unpack_from("<II", data, optional + 96 + 8 * 5)
        sites = []
        at = file_offset(rva)
        end = at + size
        while at < end:
            page, block_size = struct.unpack_from("<II", data, at)
            self.assertGreaterEqual(block_size, 8)
            self.assertEqual(block_size % 4, 0)
            for item in range(at + 8, at + block_size, 2):
                entry = struct.unpack_from("<H", data, item)[0]
                if entry:
                    self.assertEqual(entry >> 12, 3)
                    sites.append(page + (entry & 0xfff))
            at += block_size
        self.assertEqual(at, end)
        return optional, file_offset, sites

    def test_windows_relocation_directory_covers_absolute_import_thunks(self):
        obj = self.compile("i386", "--hosted=windows", "rebase-main", "int main() { return 17; }\n")
        output = BUILD / "rebase-main.exe"
        result = self.link("i386", "--hosted=windows", obj, output)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = output.read_bytes()
        optional, offset, sites = self.pe_addresses(data)
        self.assertTrue(struct.unpack_from("<H", data, optional + 70)[0] & 0x40)
        iat_rva = struct.unpack_from("<I", data, optional + 96 + 8 * 12)[0]
        expected = struct.pack("<I", 0x400000 + iat_rva)
        self.assertTrue(any(data[offset(site):offset(site) + 4] == expected for site in sites))
        self.assertNotIn(iat_rva, sites)

    def test_windows_coff_initialized_and_bss_sections_rebase_correctly(self):
        main = self.compile("i386", "--hosted=windows", "coff-sections-main",
                            "extern int imported(); int main() { return imported(); }\n")
        obj = self.coff_sections_fixture("coff-sections.obj")
        output = BUILD / "coff-sections.exe"
        result = self.link("i386", "--hosted=windows", main, output, str(obj))
        self.assertEqual(result.returncode, 0, result.stderr)
        data = output.read_bytes()
        _, offset, sites = self.pe_addresses(data)
        match = re.search(b"\xa1.{4}\x03\x05.{4}\xc3", data[0x200:], re.DOTALL)
        self.assertIsNotNone(match)
        start = 0x200 + match.start()
        datum, zero = struct.unpack_from("<I", data, start + 1)[0], struct.unpack_from("<I", data, start + 7)[0]
        self.assertEqual(struct.unpack_from("<I", data, offset(datum - 0x400000))[0], 17)
        self.assertEqual(struct.unpack_from("<I", data, offset(zero - 0x400000))[0], 0)
        for pointer in (datum, zero):
            self.assertTrue(any(struct.unpack_from("<I", data, offset(site))[0] == pointer for site in sites))
        rebased = bytearray(data)
        for site in sites:
            at = offset(site)
            struct.pack_into("<I", rebased, at, struct.unpack_from("<I", data, at)[0] + 0x100000)
        self.assertEqual(struct.unpack_from("<I", rebased, start + 1)[0], datum + 0x100000)
        self.assertEqual(struct.unpack_from("<I", rebased, start + 7)[0], zero + 0x100000)

    def test_windows_coff_unsupported_relocation_preserves_output(self):
        main = self.compile("i386", "--hosted=windows", "coff-bad-reloc-main",
                            "extern int imported(); int main() { return imported(); }\n")
        obj = self.coff_sections_fixture("coff-bad-reloc.obj")
        data = bytearray(obj.read_bytes())
        relocation_offset = struct.unpack_from("<I", data, 20 + 24)[0]
        struct.pack_into("<H", data, relocation_offset + 8, 99)
        obj.write_bytes(data)
        output = BUILD / "coff-bad-reloc.exe"
        output.write_bytes(b"keep")
        result = self.link("i386", "--hosted=windows", main, output, str(obj))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported i386 relocation 99", result.stderr)
        self.assertEqual(output.read_bytes(), b"keep")

    def test_windows_coff_rejects_unsupported_tls_initialization(self):
        self.reject_coff_runtime_section(b".tls")

    def test_windows_coff_rejects_unsupported_static_constructors(self):
        self.reject_coff_runtime_section(b".CRT$XCU")

    def test_windows_coff_relative_relocation_preserves_positive_addend(self):
        self.check_coff_relative_addend(1)

    def test_windows_coff_relative_relocation_preserves_negative_addend(self):
        self.check_coff_relative_addend(-1)

    def test_windows_coff_preserves_data_alignment_across_objects(self):
        self.check_coff_data_alignment(64, 7)

    def test_windows_coff_preserves_large_data_alignment_across_objects(self):
        self.check_coff_data_alignment(128, 8)

    def check_coff_data_alignment(self, alignment, encoding):
        main = self.compile("i386", "--hosted=windows", "coff-aligned-main",
                            "extern int imported(); int main() { return imported(); }\n")
        obj = self.coff_sections_fixture("coff-aligned.obj")
        data = bytearray(obj.read_bytes())
        struct.pack_into("<I", data, 60 + 36, 0xc0000040 | (encoding << 20))
        obj.write_bytes(data)
        output = BUILD / "coff-aligned.exe"
        result = self.link("i386", "--hosted=windows", main, output, str(obj))
        self.assertEqual(result.returncode, 0, result.stderr)
        code = output.read_bytes()[0x200:]
        match = re.search(b"\xa1.{4}\x03\x05.{4}\xc3", code, re.DOTALL)
        self.assertIsNotNone(match)
        address = struct.unpack_from("<I", code, match.start() + 1)[0]
        self.assertEqual(address % alignment, 0)

    def check_coff_relative_addend(self, addend):
        main = self.compile("i386", "--hosted=windows", "coff-addend-main",
                            "extern int imported(); int main() { return imported(); }\n")
        obj = self.coff_sections_fixture("coff-addend.obj")
        data = bytearray(obj.read_bytes())
        raw = struct.unpack_from("<I", data, 20 + 20)[0]
        data[raw:raw + 12] = b"\xe8" + struct.pack("<i", addend) + b"\xc3\x90\x90\xc3\xc3\x90\x90"
        relocation = struct.unpack_from("<I", data, 20 + 24)[0]
        struct.pack_into("<H", data, relocation + 8, 20)
        struct.pack_into("<H", data, 20 + 32, 1)
        symbols = struct.unpack_from("<I", data, 8)[0]
        struct.pack_into("<Ih", data, symbols + 18 + 8, 8, 1)
        obj.write_bytes(data)
        output = BUILD / "coff-addend.exe"
        result = self.link("i386", "--hosted=windows", main, output, str(obj))
        self.assertEqual(result.returncode, 0, result.stderr)
        code = output.read_bytes()[0x200:]
        match = re.search(b"\xe8.{4}\xc3\x90\x90\xc3\xc3\x90\x90", code, re.DOTALL)
        self.assertIsNotNone(match)
        displacement = struct.unpack_from("<i", code, match.start() + 1)[0]
        self.assertEqual(match.start() + 5 + displacement, match.start() + 8 + addend)

    def reject_coff_runtime_section(self, section_name):
        main = self.compile("i386", "--hosted=windows", "coff-runtime-main",
                            "extern int imported(); int main() { return imported(); }\n")
        obj = self.coff_sections_fixture("coff-runtime.obj")
        data = bytearray(obj.read_bytes())
        data[60:68] = section_name.ljust(8, b"\0")
        obj.write_bytes(data)
        output = BUILD / "coff-runtime.exe"
        output.write_bytes(b"keep")
        result = self.link("i386", "--hosted=windows", main, output, str(obj))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported runtime section " + section_name.decode(), result.stderr)
        self.assertEqual(output.read_bytes(), b"keep")

    def gnu_import_library(self, filename, ordinal=None, provider=True):
        def coff(sections, symbols):
            content = bytearray(20 + 40 * len(sections))
            for i, (name, raw) in enumerate(sections):
                header = 20 + i * 40
                content[header:header + 8] = name.ljust(8, b"\0")
                struct.pack_into("<IIII", content, header + 8, 0, 0, len(raw), len(content))
                struct.pack_into("<I", content, header + 36, 0x60000020 if name == b".text" else 0xc0000040)
                content += raw
            symbol_offset = len(content)
            strings = bytearray(4)
            for name, section in symbols:
                encoded = struct.pack("<II", 0, len(strings))
                strings += name + b"\0"
                content += encoded + struct.pack("<IhHBB", 0, section, 0, 2, 0)
            struct.pack_into("<I", strings, 0, len(strings))
            content += strings
            struct.pack_into("<HHIIIHH", content, 0, 0x14c, len(sections), 0, symbol_offset, len(symbols), 0, 0)
            return bytes(content)

        def member(name, data):
            fields = [name.ljust(16), "0".ljust(12), "0".ljust(6), "0".ljust(6),
                      "100644".ljust(8), str(len(data)).ljust(10), "`\n"]
            return "".join(fields).encode() + data + (b"\n" if len(data) % 2 else b"")

        sections = [(b".text", b"\xff\x25\0\0\0\0"),
                    (b".idata$5", struct.pack("<I", 0 if ordinal is None else 0x80000000 | ordinal))]
        if ordinal is None:
            sections.append((b".idata$6", b"\0\0imported\0"))
        archive = b"!<arch>\n" + member("head.o/", coff([
            (b".idata$2", bytes(20)), (b".idata$5", b""), (b".idata$4", b"")], [(b"_head_sample", 1)]))
        archive += member("entry.o/", coff(sections, [(b"_imported", 1), (b"__imp__imported", 2)]))
        if provider:
            archive += member("tail.o/", coff([(b".idata$7", b"sample.dll\0")], []))
        path = BUILD / filename
        path.write_bytes(archive)
        return path

    def test_windows_gnu_import_library_resolves_named_function(self):
        main = self.compile("i386", "--hosted=windows", "gnu-import-main",
                            "extern int imported(); int main() { return imported(); }\n")
        library = self.gnu_import_library("libgnu-import.dll.a")
        output = BUILD / "gnu-import.exe"
        result = self.link("i386", "--hosted=windows", main, output, str(library))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"sample.dll\0", output.read_bytes())
        self.assertIn(b"\0\0imported\0", output.read_bytes())

    def test_windows_gnu_import_library_resolves_ordinal_function(self):
        main = self.compile("i386", "--hosted=windows", "gnu-ordinal-main",
                            "extern int imported(); int main() { return imported(); }\n")
        library = self.gnu_import_library("libgnu-ordinal.dll.a", ordinal=19)
        output = BUILD / "gnu-ordinal.exe"
        result = self.link("i386", "--hosted=windows", main, output, str(library))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"sample.dll\0", output.read_bytes())
        self.assertIn(struct.pack("<I", 0x80000013), output.read_bytes())

    def test_windows_gnu_import_library_rejects_missing_dll_provider(self):
        main = self.compile("i386", "--hosted=windows", "gnu-missing-main",
                            "extern int imported(); int main() { return imported(); }\n")
        library = self.gnu_import_library("libgnu-missing.dll.a", provider=False)
        output = BUILD / "gnu-missing.exe"
        output.write_bytes(b"keep")
        result = self.link("i386", "--hosted=windows", main, output, str(library))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("no DLL provider", result.stderr)
        self.assertEqual(output.read_bytes(), b"keep")

    def test_windows_external_linker_selection_is_rejected(self):
        obj = self.compile("i386", "--hosted=windows", "unsupported-pe-main", "int main() { return 0; }\n")
        output = BUILD / "preserved-pe-output"
        output.write_bytes(b"keep this PE file")
        result = subprocess.run(
            [str(PCC), "--link", "--target=i386", "--hosted=windows", "--format=coff",
             "--windows-linker=mingw", f"--out={output}", str(obj)],
            capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("hosted linking is internal", result.stderr)
        self.assertEqual(output.read_bytes(), b"keep this PE file")

    def test_unresolved_symbol_failure_preserves_existing_output(self):
        obj = self.compile("i386", "--hosted", "unresolved-main",
                           "extern int absent(); int main() { return absent(); }\n")
        output = BUILD / "preserved-unresolved-output"
        output.write_bytes(b"original output")
        result = self.link("i386", "--hosted", obj, output)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unresolved symbol", result.stderr)
        self.assertEqual(output.read_bytes(), b"original output")

    def test_shared_library_input_has_an_explicit_diagnostic(self):
        obj = self.compile("i386", "--hosted", "shared-main", "int main() { return 0; }\n")
        shared = BUILD / "libunsupported.so"
        shared.write_text("not a supported static archive")
        output = BUILD / "preserved-shared-output"
        output.write_bytes(b"original output")
        result = self.link("i386", "--hosted", obj, output, str(shared))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported GNU linker script directive", result.stderr)
        self.assertEqual(output.read_bytes(), b"original output")


if __name__ == "__main__":
    unittest.main()
