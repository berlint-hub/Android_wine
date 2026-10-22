import hashlib
import struct
import unittest

from android_elf_inventory import ELFInventoryError, ELFParser


def make_test_elf():
    size = 0x900
    dynamic_offset = 0x100
    strings_offset = 0x300
    symbols_offset = 0x400
    gnu_hash_offset = 0x500
    versym_offset = 0x540
    verneed_offset = 0x560
    rela_offset = 0x600
    jmprel_offset = rela_offset + 24
    fini_array_offset = 0x700

    strings = bytearray(b"\0")
    string_offsets = {}
    for string in ("libandroid.so", "libvulkan.so", "libm.so", "libdl.so",
                   "libc.so", "libfixture.so", "foo", "probe", "LIBC"):
        string_offsets[string] = len(strings)
        strings.extend(string.encode("ascii") + b"\0")

    tags = [
        (1, string_offsets["libandroid.so"]),
        (1, string_offsets["libvulkan.so"]),
        (1, string_offsets["libm.so"]),
        (1, string_offsets["libdl.so"]),
        (1, string_offsets["libc.so"]),
        (14, string_offsets["libfixture.so"]),
        (5, strings_offset),
        (10, len(strings)),
        (6, symbols_offset),
        (11, 24),
        (0x6FFFFEF5, gnu_hash_offset),
        (7, rela_offset),
        (8, 24),
        (9, 24),
        (23, jmprel_offset),
        (2, 24),
        (20, 7),
        (30, 8),
        (26, fini_array_offset),
        (28, 16),
        (0x6FFFFFF0, versym_offset),
        (0x6FFFFFFE, verneed_offset),
        (0x6FFFFFFF, 1),
        (0, 0),
    ]

    data = bytearray(size)
    ident = b"\x7fELF\x02\x01\x01\x00" + b"\0" * 8
    header = struct.pack("<16sHHIQQQIHHHHHH", ident, 3, 183, 1, 0, 64, 0,
                         0, 64, 56, 2, 0, 0, 0)
    data[:64] = header

    load_phdr = struct.pack("<IIQQQQQQ", 1, 7, 0, 0, 0, size, 0x1000, 0x1000)
    dynamic_size = len(tags) * 16
    dynamic_phdr = struct.pack("<IIQQQQQQ", 2, 6, dynamic_offset, dynamic_offset,
                               0, dynamic_size, dynamic_size, 8)
    data[64:120] = load_phdr
    data[120:176] = dynamic_phdr

    for i, (tag, value) in enumerate(tags):
        data[dynamic_offset + i * 16 : dynamic_offset + (i + 1) * 16] = struct.pack(
            "<qQ", tag, value
        )
    data[strings_offset : strings_offset + len(strings)] = strings

    # GNU hash: symbol index 2 is the first hashable symbol and ends its chain.
    data[gnu_hash_offset : gnu_hash_offset + 16] = struct.pack("<IIII", 1, 2, 1, 5)
    data[gnu_hash_offset + 16 : gnu_hash_offset + 24] = struct.pack("<Q", 0)
    data[gnu_hash_offset + 24 : gnu_hash_offset + 28] = struct.pack("<I", 2)
    data[gnu_hash_offset + 28 : gnu_hash_offset + 32] = struct.pack("<I", 1)

    data[symbols_offset : symbols_offset + 24] = b"\0" * 24
    data[symbols_offset + 24 : symbols_offset + 48] = struct.pack(
        "<IBBHQQ", string_offsets["foo"], 0x12, 0, 0, 0, 0
    )
    data[symbols_offset + 48 : symbols_offset + 72] = struct.pack(
        "<IBBHQQ", string_offsets["probe"], 0x12, 0, 1, 0x850, 16
    )
    data[versym_offset : versym_offset + 6] = struct.pack("<HHH", 0, 2, 1)
    data[verneed_offset : verneed_offset + 16] = struct.pack(
        "<HHIII", 1, 1, string_offsets["libc.so"], 16, 0
    )
    data[verneed_offset + 16 : verneed_offset + 32] = struct.pack(
        "<IHHII", 0, 0, 2, string_offsets["LIBC"], 0
    )
    data[rela_offset : rela_offset + 24] = struct.pack(
        "<QQq", fini_array_offset, 1027, 0x800
    )
    data[jmprel_offset : jmprel_offset + 24] = struct.pack(
        "<QQq", 0x800, (1 << 32) | 1026, 0
    )
    return bytes(data), {
        "dynamic": dynamic_offset,
        "dynamic_count": len(tags),
        "gnu_chain": gnu_hash_offset + 28,
        "plt_relocation": jmprel_offset,
        "machine": 18,
    }


class ELFInventoryTests(unittest.TestCase):
    def setUp(self):
        self.data, self.offsets = make_test_elf()

    def test_inventory_reports_fixture_abi_imports_and_relocations(self):
        report = ELFParser(self.data).inventory()
        self.assertEqual(report.sha256, hashlib.sha256(self.data).hexdigest())
        self.assertEqual((report.elf_class, report.endianness, report.elf_type, report.machine),
                         (2, "little", 3, 183))
        self.assertEqual(report.soname, "libfixture.so")
        self.assertEqual(report.needed,
                         ["libandroid.so", "libvulkan.so", "libm.so", "libdl.so", "libc.so"])
        self.assertTrue(report.bind_now)
        self.assertEqual(report.exports, ["probe"])
        imports = {symbol.name: symbol.version for symbol in report.symbols}
        self.assertEqual(imports, {"foo": "LIBC"})
        self.assertEqual([rel.kind_name for rel in report.relocations],
                         ["R_AARCH64_RELATIVE", "R_AARCH64_JUMP_SLOT"])
        self.assertEqual(report.relocations[1].symbol, "foo")
        self.assertEqual(report.fini_array_size, 16)

    def test_rejects_bad_magic(self):
        data = bytearray(self.data)
        data[0] = 0
        with self.assertRaisesRegex(ELFInventoryError, "magic"):
            ELFParser(bytes(data))

    def test_rejects_non_aarch64(self):
        data = bytearray(self.data)
        struct.pack_into("<H", data, 18, 62)
        with self.assertRaisesRegex(ELFInventoryError, "AArch64"):
            ELFParser(bytes(data))

    def test_rejects_truncated_program_header_table(self):
        with self.assertRaisesRegex(ELFInventoryError, "program-header table"):
            ELFParser(self.data[:100])

    def test_rejects_out_of_range_string_table_pointer(self):
        data = bytearray(self.data)
        struct.pack_into("<Q", data, self.offsets["dynamic"] + 6 * 16 + 8, 0x2000)
        with self.assertRaisesRegex(ELFInventoryError, "dynamic string table"):
            ELFParser(bytes(data))

    def test_rejects_unterminated_dynamic_table(self):
        data = bytearray(self.data)
        null_tag_offset = (self.offsets["dynamic"] +
                           (self.offsets["dynamic_count"] - 1) * 16)
        struct.pack_into("<q", data, null_tag_offset, 0x123456)
        with self.assertRaisesRegex(ELFInventoryError, "DT_NULL"):
            ELFParser(bytes(data))

    def test_rejects_out_of_range_gnu_hash_chain(self):
        data = bytearray(self.data)
        struct.pack_into("<I", data, self.offsets["gnu_chain"] - 4, 0x7FFFFFFF)
        with self.assertRaisesRegex(ELFInventoryError, "GNU hash chain index"):
            ELFParser(bytes(data))

    def test_rejects_relocation_target_outside_load_segments(self):
        data = bytearray(self.data)
        struct.pack_into("<Q", data, self.offsets["plt_relocation"], 0x2000)
        with self.assertRaisesRegex(ELFInventoryError, "target is outside"):
            ELFParser(bytes(data)).inventory()

    def test_rejects_relocation_with_invalid_symbol_index(self):
        data = bytearray(self.data)
        struct.pack_into("<Q", data, self.offsets["plt_relocation"] + 8,
                         (99 << 32) | 1026)
        with self.assertRaisesRegex(ELFInventoryError, "invalid symbol"):
            ELFParser(bytes(data)).inventory()


if __name__ == "__main__":
    unittest.main()
