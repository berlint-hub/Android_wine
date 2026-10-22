#!/usr/bin/env python3
"""Bounds-checked inventory of the AndroidWine loader fixture's ELF metadata.

This tool parses metadata only. It never maps or executes ELF segments.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable

MAX_FILE_SIZE = 64 * 1024 * 1024
MAX_PROGRAM_HEADERS = 128
MAX_DYNAMIC_ENTRIES = 65_536
MAX_SYMBOLS = 100_000
MAX_GNU_HASH_STEPS = 4_000_000
MAX_VERSION_RECORDS = 4_096

ET_DYN = 3
EM_AARCH64 = 183
PT_LOAD = 1
PT_DYNAMIC = 2
PF_X = 1
PF_W = 2
PF_R = 4

DT_NULL = 0
DT_NEEDED = 1
DT_PLTRELSZ = 2
DT_STRTAB = 5
DT_SYMTAB = 6
DT_RELA = 7
DT_RELASZ = 8
DT_RELAENT = 9
DT_STRSZ = 10
DT_SYMENT = 11
DT_SONAME = 14
DT_PLTREL = 20
DT_BIND_NOW = 24
DT_FLAGS = 30
DT_FINI_ARRAY = 26
DT_FINI_ARRAYSZ = 28
DT_GNU_HASH = 0x6FFFFEF5
DT_VERSYM = 0x6FFFFFF0
DT_FLAGS_1 = 0x6FFFFFFB
DT_VERNEED = 0x6FFFFFFE
DT_VERNEEDNUM = 0x6FFFFFFF
DT_JMPREL = 23
DT_RELACOUNT = 0x6FFFFFF9

DF_BIND_NOW = 0x8
DF_1_NOW = 0x1
DT_RELA_VALUE = 7

R_AARCH64_ABS64 = 257
R_AARCH64_GLOB_DAT = 1025
R_AARCH64_JUMP_SLOT = 1026
R_AARCH64_RELATIVE = 1027

DYNAMIC_NAMES = {
    DT_NULL: "DT_NULL",
    DT_NEEDED: "DT_NEEDED",
    DT_PLTRELSZ: "DT_PLTRELSZ",
    DT_STRTAB: "DT_STRTAB",
    DT_SYMTAB: "DT_SYMTAB",
    DT_RELA: "DT_RELA",
    DT_RELASZ: "DT_RELASZ",
    DT_RELAENT: "DT_RELAENT",
    DT_STRSZ: "DT_STRSZ",
    DT_SYMENT: "DT_SYMENT",
    DT_SONAME: "DT_SONAME",
    DT_PLTREL: "DT_PLTREL",
    DT_BIND_NOW: "DT_BIND_NOW",
    DT_FLAGS: "DT_FLAGS",
    DT_FINI_ARRAY: "DT_FINI_ARRAY",
    DT_FINI_ARRAYSZ: "DT_FINI_ARRAYSZ",
    DT_JMPREL: "DT_JMPREL",
    DT_GNU_HASH: "DT_GNU_HASH",
    DT_VERSYM: "DT_VERSYM",
    DT_FLAGS_1: "DT_FLAGS_1",
    DT_VERNEED: "DT_VERNEED",
    DT_VERNEEDNUM: "DT_VERNEEDNUM",
    DT_RELACOUNT: "DT_RELACOUNT",
}
RELOCATION_NAMES = {
    R_AARCH64_ABS64: "R_AARCH64_ABS64",
    R_AARCH64_GLOB_DAT: "R_AARCH64_GLOB_DAT",
    R_AARCH64_JUMP_SLOT: "R_AARCH64_JUMP_SLOT",
    R_AARCH64_RELATIVE: "R_AARCH64_RELATIVE",
}


class ELFInventoryError(ValueError):
    """The file is malformed or outside the inspector's deliberately narrow scope."""


@dataclass(frozen=True)
class ProgramHeader:
    p_type: int
    p_flags: int
    p_offset: int
    p_vaddr: int
    p_filesz: int
    p_memsz: int
    p_align: int


@dataclass(frozen=True)
class DynamicSymbol:
    index: int
    name: str
    binding: int
    kind: int
    section_index: int
    version: str | None
    version_file: str | None


@dataclass(frozen=True)
class Relocation:
    table: str
    offset: int
    kind: int
    kind_name: str
    symbol_index: int
    symbol: str | None
    addend: int


@dataclass(frozen=True)
class ELFInventory:
    sha256: str
    file_size: int
    elf_class: int
    endianness: str
    elf_type: int
    machine: int
    soname: str | None
    needed: list[str]
    bind_now: bool
    dynamic_tags: list[str]
    symbols: list[DynamicSymbol]
    exports: list[str]
    relocations: list[Relocation]
    fini_array_size: int

    def to_dict(self) -> dict:
        return asdict(self)


def _checked_range(data: bytes, offset: int, size: int, label: str) -> memoryview:
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise ELFInventoryError(f"{label} lies outside the file")
    return memoryview(data)[offset : offset + size]


def _cstring(table: bytes, index: int, label: str) -> str:
    if index < 0 or index >= len(table):
        raise ELFInventoryError(f"{label} string offset is out of range")
    end = table.find(b"\0", index)
    if end < 0:
        raise ELFInventoryError(f"{label} string is not NUL-terminated")
    return table[index:end].decode("utf-8", errors="replace")


class ELFParser:
    def __init__(self, data: bytes):
        if len(data) > MAX_FILE_SIZE:
            raise ELFInventoryError(f"file exceeds {MAX_FILE_SIZE} byte inspection limit")
        _checked_range(data, 0, 64, "ELF header")
        self.data = data
        self.program_headers: list[ProgramHeader] = []
        self.dynamic: dict[int, list[int]] = {}
        self.strings: bytes
        self.symbols: list[DynamicSymbol]
        self.versym: list[int] = []
        self.version_names: dict[int, tuple[str, str]] = {}
        self.elf_type = 0
        self.machine = 0
        self.e_phoff = 0
        self.e_phentsize = 0
        self.e_phnum = 0
        self._parse_header_and_program_headers()
        self._parse_dynamic_section()
        self._parse_string_table()
        self._parse_symbol_table()
        self._parse_versions()
        self.symbols = self._materialize_symbols()

    def _unpack(self, fmt: str, offset: int, label: str) -> tuple:
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, _checked_range(self.data, offset, size, label))

    def _single(self, tag: int, *, required: bool = True) -> int | None:
        values = self.dynamic.get(tag, [])
        if len(values) > 1:
            raise ELFInventoryError(f"duplicate {DYNAMIC_NAMES.get(tag, hex(tag))} tag")
        if not values:
            if required:
                raise ELFInventoryError(f"missing {DYNAMIC_NAMES.get(tag, hex(tag))} tag")
            return None
        return values[0]

    def _parse_header_and_program_headers(self) -> None:
        ident = bytes(_checked_range(self.data, 0, 16, "ELF identification"))
        if ident[:4] != b"\x7fELF":
            raise ELFInventoryError("invalid ELF magic")
        if ident[4] != 2:
            raise ELFInventoryError("only ELF64 is supported by this inspector")
        if ident[5] != 1:
            raise ELFInventoryError("only little-endian ELF is supported by this inspector")
        if ident[6] != 1:
            raise ELFInventoryError("unsupported ELF identification version")

        (self.elf_type, self.machine, version, _entry, self.e_phoff, _shoff,
         _flags, ehsize, self.e_phentsize, self.e_phnum, _shentsize, _shnum,
         _shstrndx) = self._unpack("<HHIQQQIHHHHHH", 16, "ELF header")
        if version != 1 or ehsize != 64:
            raise ELFInventoryError("invalid ELF header version or size")
        if self.elf_type != ET_DYN:
            raise ELFInventoryError("only ET_DYN shared objects are supported")
        if self.machine != EM_AARCH64:
            raise ELFInventoryError("only AArch64 shared objects are supported")
        if self.e_phnum == 0 or self.e_phnum > MAX_PROGRAM_HEADERS or self.e_phentsize != 56:
            raise ELFInventoryError("invalid or unsupported program-header table")
        _checked_range(self.data, self.e_phoff, self.e_phentsize * self.e_phnum,
                       "program-header table")

        for i in range(self.e_phnum):
            fields = self._unpack("<IIQQQQQQ", self.e_phoff + i * self.e_phentsize,
                                  f"program header {i}")
            p_type, p_flags, p_offset, p_vaddr, _paddr, p_filesz, p_memsz, p_align = fields
            if p_filesz > p_memsz:
                raise ELFInventoryError(f"program header {i} has p_filesz > p_memsz")
            if p_offset > len(self.data) or p_filesz > len(self.data) - p_offset:
                raise ELFInventoryError(f"program header {i} file range is out of bounds")
            if p_type == PT_LOAD:
                if p_align not in (0, 1) and (p_align & (p_align - 1)) != 0:
                    raise ELFInventoryError(f"PT_LOAD {i} has non-power-of-two alignment")
                if p_align > 1 and p_offset % p_align != p_vaddr % p_align:
                    raise ELFInventoryError(f"PT_LOAD {i} offset/address alignment mismatch")
            self.program_headers.append(
                ProgramHeader(p_type, p_flags, p_offset, p_vaddr, p_filesz, p_memsz, p_align)
            )

        if not any(ph.p_type == PT_LOAD for ph in self.program_headers):
            raise ELFInventoryError("ELF has no PT_LOAD segment")
        if sum(ph.p_type == PT_DYNAMIC for ph in self.program_headers) != 1:
            raise ELFInventoryError("ELF must contain exactly one PT_DYNAMIC segment")

    def _map_vaddr(self, address: int, size: int, label: str) -> tuple[int, int]:
        if address < 0 or size < 0:
            raise ELFInventoryError(f"invalid virtual range for {label}")
        matches = []
        for ph in self.program_headers:
            if ph.p_type != PT_LOAD or address < ph.p_vaddr:
                continue
            delta = address - ph.p_vaddr
            if delta <= ph.p_filesz and size <= ph.p_filesz - delta:
                matches.append((ph.p_offset + delta, ph.p_filesz - delta))
        if len(matches) != 1:
            raise ELFInventoryError(f"{label} is not in one unambiguous file-backed PT_LOAD range")
        offset, available = matches[0]
        _checked_range(self.data, offset, size, label)
        return offset, available

    def _parse_dynamic_section(self) -> None:
        ph = next(ph for ph in self.program_headers if ph.p_type == PT_DYNAMIC)
        if ph.p_filesz == 0 or ph.p_filesz % 16 != 0:
            raise ELFInventoryError("invalid PT_DYNAMIC size")
        entries = min(ph.p_filesz // 16, MAX_DYNAMIC_ENTRIES)
        terminated = False
        for i in range(entries):
            tag, value = self._unpack("<qQ", ph.p_offset + i * 16, f"dynamic entry {i}")
            if tag == DT_NULL:
                terminated = True
                break
            self.dynamic.setdefault(tag, []).append(value)
        if not terminated:
            raise ELFInventoryError("dynamic table has no DT_NULL terminator")

    def _parse_string_table(self) -> None:
        address = self._single(DT_STRTAB)
        size = self._single(DT_STRSZ)
        assert address is not None and size is not None
        if size == 0 or size > MAX_FILE_SIZE:
            raise ELFInventoryError("invalid dynamic string-table size")
        offset, _ = self._map_vaddr(address, size, "dynamic string table")
        self.strings = self.data[offset : offset + size]

    def _read_string(self, index: int, label: str) -> str:
        return _cstring(self.strings, index, label)

    def _parse_gnu_hash_symbol_count(self) -> int:
        address = self._single(DT_GNU_HASH)
        assert address is not None
        base, available = self._map_vaddr(address, 16, "GNU hash header")
        nbuckets, symoffset, bloom_size, _bloom_shift = self._unpack(
            "<IIII", base, "GNU hash header"
        )
        if not 0 < nbuckets <= MAX_SYMBOLS or not 0 < bloom_size <= MAX_SYMBOLS:
            raise ELFInventoryError("invalid GNU hash dimensions")
        if not 1 <= symoffset <= MAX_SYMBOLS:
            raise ELFInventoryError("invalid GNU hash symbol offset")
        bloom_bytes = bloom_size * 8
        bucket_bytes = nbuckets * 4
        chain_offset = 16 + bloom_bytes + bucket_bytes
        if chain_offset > available:
            raise ELFInventoryError("GNU hash bloom/bucket arrays exceed PT_LOAD range")
        buckets_offset = base + 16 + bloom_bytes
        buckets = [self._unpack("<I", buckets_offset + i * 4, "GNU hash bucket")[0]
                   for i in range(nbuckets)]
        max_symbol = symoffset - 1
        chain_capacity = (available - chain_offset) // 4
        chain_steps = 0
        for bucket in buckets:
            if bucket == 0:
                continue
            if bucket < symoffset:
                raise ELFInventoryError("GNU hash bucket precedes symbol offset")
            symbol = bucket
            for _ in range(chain_capacity):
                chain_steps += 1
                if chain_steps > MAX_GNU_HASH_STEPS:
                    raise ELFInventoryError("GNU hash traversal exceeds inspection limit")
                chain_index = symbol - symoffset
                if chain_index >= chain_capacity:
                    raise ELFInventoryError("GNU hash chain index is out of range")
                chain = self._unpack("<I", base + chain_offset + chain_index * 4,
                                     "GNU hash chain")[0]
                max_symbol = max(max_symbol, symbol)
                if chain & 1:
                    break
                symbol += 1
                if symbol >= MAX_SYMBOLS:
                    raise ELFInventoryError("GNU hash chain exceeds symbol-count limit")
            else:
                raise ELFInventoryError("unterminated GNU hash chain")
        count = max(symoffset, max_symbol + 1)
        if count > MAX_SYMBOLS:
            raise ELFInventoryError("dynamic symbol count exceeds inspection limit")
        return count

    def _parse_symbol_table(self) -> None:
        symtab_address = self._single(DT_SYMTAB)
        syment = self._single(DT_SYMENT)
        assert symtab_address is not None and syment is not None
        if syment != 24:
            raise ELFInventoryError("unexpected ELF64 dynamic symbol entry size")
        self._symtab_address = symtab_address
        self._symbol_count = self._parse_gnu_hash_symbol_count()
        self._map_vaddr(symtab_address, self._symbol_count * syment, "dynamic symbol table")

    def _parse_versions(self) -> None:
        versym_address = self._single(DT_VERSYM, required=False)
        verneed_address = self._single(DT_VERNEED, required=False)
        verneed_count = self._single(DT_VERNEEDNUM, required=False)
        if (verneed_address is None) != (verneed_count is None):
            raise ELFInventoryError("DT_VERNEED and DT_VERNEEDNUM must appear together")
        if verneed_count is not None:
            if verneed_count > MAX_VERSION_RECORDS:
                raise ELFInventoryError("too many version-need records")
            address = verneed_address
            assert address is not None
            for i in range(verneed_count):
                offset, _ = self._map_vaddr(address, 16, "version-need record")
                vn_version, vn_cnt, vn_file, vn_aux, vn_next = self._unpack(
                    "<HHIII", offset, "version-need record"
                )
                if vn_version != 1 or vn_cnt > MAX_VERSION_RECORDS or vn_aux == 0 and vn_cnt:
                    raise ELFInventoryError("invalid version-need record")
                file_name = self._read_string(vn_file, "version-need file")
                aux_address = address + vn_aux
                for j in range(vn_cnt):
                    aux_offset, _ = self._map_vaddr(aux_address, 16, "version auxiliary record")
                    _hash, _flags, other, name_index, next_aux = self._unpack(
                        "<IHHII", aux_offset, "version auxiliary record"
                    )
                    version_index = other & 0x7FFF
                    if version_index <= 1:
                        raise ELFInventoryError("invalid version-need index")
                    self.version_names[version_index] = (
                        self._read_string(name_index, "version name"), file_name
                    )
                    if j + 1 < vn_cnt:
                        if next_aux == 0:
                            raise ELFInventoryError("truncated version auxiliary chain")
                        aux_address += next_aux
                if i + 1 < verneed_count:
                    if vn_next == 0:
                        raise ELFInventoryError("truncated version-need chain")
                    address += vn_next
        if versym_address is not None:
            offset, _ = self._map_vaddr(versym_address, self._symbol_count * 2,
                                         "version-symbol table")
            self.versym = [self._unpack("<H", offset + i * 2, "version-symbol entry")[0]
                           for i in range(self._symbol_count)]

    def _materialize_symbols(self) -> list[DynamicSymbol]:
        syment = self._single(DT_SYMENT)
        assert syment is not None
        base, _ = self._map_vaddr(self._symtab_address, self._symbol_count * syment,
                                  "dynamic symbol table")
        symbols = []
        for i in range(self._symbol_count):
            offset = base + i * syment
            st_name, st_info, _other, st_shndx, _value, _size = self._unpack(
                "<IBBHQQ", offset, f"dynamic symbol {i}"
            )
            name = self._read_string(st_name, f"dynamic symbol {i} name") if st_name else ""
            version = None
            version_file = None
            if self.versym:
                version_index = self.versym[i] & 0x7FFF
                if version_index > 1:
                    item = self.version_names.get(version_index)
                    if item is None:
                        raise ELFInventoryError(f"unresolved symbol version index {version_index}")
                    version, version_file = item
            symbols.append(DynamicSymbol(i, name, st_info >> 4, st_info & 0xF,
                                         st_shndx, version, version_file))
        return symbols

    def _validate_relocation_target(self, address: int, size: int, label: str) -> None:
        matches = []
        for ph in self.program_headers:
            if ph.p_type != PT_LOAD or address < ph.p_vaddr:
                continue
            delta = address - ph.p_vaddr
            if delta <= ph.p_memsz and size <= ph.p_memsz - delta:
                matches.append(ph)
        if len(matches) != 1:
            raise ELFInventoryError(f"{label} target is outside or ambiguous across PT_LOAD segments")

    def _parse_relocations(self, address: int, size: int, table: str) -> list[Relocation]:
        if size == 0:
            return []
        entry_size = self._single(DT_RELAENT)
        if entry_size != 24 or size % entry_size != 0:
            raise ELFInventoryError(f"invalid {table} RELA table size")
        count = size // entry_size
        if count > MAX_SYMBOLS:
            raise ELFInventoryError(f"{table} relocation count exceeds inspection limit")
        offset, _ = self._map_vaddr(address, size, f"{table} relocation table")
        result = []
        for i in range(count):
            r_offset, r_info, addend = self._unpack("<QQq", offset + i * entry_size,
                                                     f"{table} relocation {i}")
            symbol_index = r_info >> 32
            kind = r_info & 0xFFFFFFFF
            self._validate_relocation_target(r_offset, 8, f"{table} relocation {i}")
            if symbol_index >= self._symbol_count:
                raise ELFInventoryError(f"{table} relocation references an invalid symbol")
            symbol = self.symbols[symbol_index].name if symbol_index else None
            result.append(Relocation(table, r_offset, kind,
                                     RELOCATION_NAMES.get(kind, f"R_AARCH64_{kind}"),
                                     symbol_index, symbol, addend))
        return result

    def inventory(self) -> ELFInventory:
        needed = [self._read_string(index, "DT_NEEDED")
                  for index in self.dynamic.get(DT_NEEDED, [])]
        soname_index = self._single(DT_SONAME, required=False)
        soname = self._read_string(soname_index, "DT_SONAME") if soname_index is not None else None
        rela_address = self._single(DT_RELA, required=False)
        rela_size = self._single(DT_RELASZ, required=False)
        jmprel_address = self._single(DT_JMPREL, required=False)
        pltrelsz = self._single(DT_PLTRELSZ, required=False)
        if (rela_address is None) != (rela_size is None):
            raise ELFInventoryError("DT_RELA and DT_RELASZ must appear together")
        if (jmprel_address is None) != (pltrelsz is None):
            raise ELFInventoryError("DT_JMPREL and DT_PLTRELSZ must appear together")
        relocations: list[Relocation] = []
        if rela_address is not None and rela_size:
            relocations += self._parse_relocations(rela_address, rela_size, ".rela.dyn")
        if jmprel_address is not None and pltrelsz:
            if self._single(DT_PLTREL) != DT_RELA_VALUE:
                raise ELFInventoryError("only RELA PLT relocations are supported")
            relocations += self._parse_relocations(jmprel_address, pltrelsz, ".rela.plt")

        flags = self._single(DT_FLAGS, required=False) or 0
        flags_1 = self._single(DT_FLAGS_1, required=False) or 0
        bind_now = bool(flags & DF_BIND_NOW or flags_1 & DF_1_NOW or
                        DT_BIND_NOW in self.dynamic)
        imports = [symbol for symbol in self.symbols if symbol.section_index == 0 and symbol.name]
        exports = [symbol.name for symbol in self.symbols
                   if symbol.section_index != 0 and symbol.name and symbol.binding in (1, 2)]
        fini_address = self._single(DT_FINI_ARRAY, required=False)
        fini_size = self._single(DT_FINI_ARRAYSZ, required=False)
        if (fini_address is None) != (fini_size is None):
            raise ELFInventoryError("DT_FINI_ARRAY and DT_FINI_ARRAYSZ must appear together")
        if fini_size is not None:
            if fini_size % 8 != 0:
                raise ELFInventoryError("DT_FINI_ARRAYSZ is not pointer-aligned")
            if fini_size:
                self._map_vaddr(fini_address, fini_size, "DT_FINI_ARRAY")
        dynamic_tags = sorted({DYNAMIC_NAMES.get(tag, f"0x{tag:x}")
                               for tag in self.dynamic})
        return ELFInventory(
            hashlib.sha256(self.data).hexdigest(), len(self.data), 2, "little", self.elf_type,
            self.machine, soname, needed, bind_now, dynamic_tags, imports, exports,
            relocations, self._single(DT_FINI_ARRAYSZ, required=False) or 0,
        )


def inspect_file(path: Path) -> ELFInventory:
    try:
        size = path.stat().st_size
        if size > MAX_FILE_SIZE:
            raise ELFInventoryError(f"file exceeds {MAX_FILE_SIZE} byte inspection limit")
        data = path.read_bytes()
    except OSError as exc:
        raise ELFInventoryError(f"cannot read {path}: {exc}") from exc
    if len(data) != size:
        raise ELFInventoryError("file size changed while being read")
    parser = ELFParser(data)
    return parser.inventory()


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path, help="ELF file to inspect; it is never executed")
    parser.add_argument("--pretty", action="store_true", help="pretty-print JSON")
    args = parser.parse_args(argv)
    try:
        inventory = inspect_file(args.elf)
    except ELFInventoryError as exc:
        print(f"ELF inventory rejected: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(inventory.to_dict(), indent=2 if args.pretty else None, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
