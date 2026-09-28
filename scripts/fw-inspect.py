#!/usr/bin/env python3
"""Inspect decoded NDS32 firmware offline; addresses are code addresses, not MVA offsets."""
import argparse
from bisect import bisect_left
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / 'firmware/decoded/306007/code.bin'
INSN = re.compile(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)(\S+)\s*(.*)$')
NUMBER = r'-?(?:0x[0-9a-fA-F]+|[0-9]+)'


def number(value):
    return int(value, 0)


class Firmware:
    def __init__(self, code, assembly=None, base=0, gp=0x2000d2f8):
        self.code = Path(code)
        self.data = self.code.read_bytes()
        self.assembly, self.base, self.gp = assembly, base, gp
        self._instructions = None

    def read(self, address, length):
        offset = address - self.base
        if length < 0 or offset < 0 or offset + length > len(self.data):
            raise ValueError(f'range {address:#x}+{length:#x} is outside code image')
        return self.data[offset:offset + length]

    def find(self, needle):
        if not needle:
            raise ValueError('search pattern must not be empty')
        start = 0
        while (start := self.data.find(needle, start)) >= 0:
            yield self.base + start
            start += 1

    def instructions(self):
        if self._instructions is None:
            assembly = self.assembly or self.code.with_suffix('.nds32.S')
            if Path(assembly).exists():
                text = Path(assembly).read_text()
            elif self.assembly:
                raise ValueError(f'assembly file does not exist: {assembly}')
            else:
                text = subprocess.check_output([
                    'nds32le-elf-objdump', '-D', '-b', 'binary', '-m', 'nds32', '-EL',
                    f'--adjust-vma={self.base}', str(self.code)], text=True)
            self._instructions = []
            for line in text.splitlines():
                match = INSN.match(line)
                if match:
                    address, raw, op, args = match.groups()
                    self._instructions.append((int(address, 16), bytes.fromhex(raw), op, args, line))
        return self._instructions

    def references(self, target):
        # Only follow straight-line register construction. Stop tracking on a
        # call/branch or register overwrite; these are references, not symbols.
        registers = {}
        for address, raw, op, args, _ in self.instructions():
            operands = args.split('!', 1)[0].strip()
            dest = re.match(r'(\$r\d+|\$fp|\$gp),', operands)
            reg = dest[1] if dest else None
            previous = registers.pop(reg, None)
            value, origin, kind = None, address, 'immediate'
            high = re.fullmatch(r'(\$\w+), #(' + NUMBER + ')', operands)
            combine = re.fullmatch(r'(\$\w+), (\$\w+), #(' + NUMBER + ')', operands)
            gp = re.search(r'\[.*?#(' + NUMBER + r')\]', operands)
            if op == 'sethi' and high:
                value = number(high[2]) << 12
            elif op in ('movi', 'movi55', 'movpi45') and high:
                value = number(high[2])
            elif op in ('ori', 'addi') and combine:
                source = previous if combine[2] == reg else registers.get(combine[2])
                if source:
                    old, origin = source
                    value = old | number(combine[3]) if op == 'ori' else old + number(combine[3])
            elif op == 'addi.gp' and high:
                value, kind = self.gp + number(high[2]), 'gp_address'
            elif '.gp' in op and gp:
                if self.gp + number(gp[1]) == target:
                    yield {'address': address, 'kind': 'gp_memory', 'instruction': f'{op} {args}'}
            if value is not None:
                value &= 0xffffffff
                if value == target:
                    yield {'address': address, 'origin': origin, 'kind': kind,
                           'instruction': f'{op} {args}'}
                if reg:
                    registers[reg] = value, origin
            if op.startswith(('j', 'b', 'ret')) and op not in ('bitci', 'bset', 'bclr', 'btst'):
                registers.clear()


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--code', type=Path, default=DEFAULT)
    p.add_argument('--asm', type=Path, help='Existing objdump text; generated in memory if absent')
    p.add_argument('--base', type=number, default=0, help='Load address of code.bin')
    p.add_argument('--gp', type=number, default=0x2000d2f8, help='Stock 306007 GP by default')
    p.add_argument('--json', action='store_true')
    sub = p.add_subparsers(dest='action', required=True)
    r = sub.add_parser('disasm', aliases=['range'], help='Disassemble [start,end)')
    r.add_argument('start', type=number)
    r.add_argument('end', type=number)
    x = sub.add_parser('xref', help='Find address construction and GP memory references')
    x.add_argument('text', nargs='?', help='Literal UTF-8 string to locate and cross-reference')
    x.add_argument('--address', type=number)
    c = sub.add_parser('callers', help='Direct call sites; optionally include direct jumps')
    c.add_argument('address', type=number)
    c.add_argument('--jumps', action='store_true')
    f = sub.add_parser('find', help='Find every occurrence, including overlapping matches')
    group = f.add_mutually_exclusive_group(required=True)
    group.add_argument('--hex')
    group.add_argument('--text')
    r = sub.add_parser('read', help='Read bytes at a code address')
    r.add_argument('address', type=number)
    r.add_argument('length', type=number)
    t = sub.add_parser('table', help='Read integer/pointer tables; little endian by default')
    t.add_argument('address', type=number)
    t.add_argument('--count', type=int, required=True)
    t.add_argument('--type', choices=['u8', 'u16', 'u32', 'i16', 'i32', 'pointer'], default='pointer')
    t.add_argument('--stride', type=number)
    t.add_argument('--big-endian', action='store_true')
    t.add_argument('--strings', action='store_true', help='Resolve pointers to bounded C strings')
    g = sub.add_parser('gp', help='Resolve a signed GP offset without reading the device')
    g.add_argument('offset', type=number, help='Use -- before a negative hexadecimal offset')
    return p


def inspect(a):
    if a.action == 'gp':
        return [{'gp': a.gp, 'offset': a.offset, 'address': (a.gp + a.offset) & 0xffffffff}]
    f = Firmware(a.code, a.asm, a.base, a.gp)
    if a.action in ('disasm', 'range'):
        f.read(a.start, a.end - a.start)
        ins = f.instructions()
        start = bisect_left([i[0] for i in ins], a.start)
        rows = []
        for addr, raw, op, args, _ in ins[start:]:
            if addr >= a.end:
                break
            rows.append({'address': addr, 'hex': raw.hex(), 'instruction': f'{op} {args}'.rstrip()})
        return rows
    if a.action == 'find':
        needle = bytes.fromhex(a.hex) if a.hex is not None else a.text.encode()
        return [{'address': addr, 'length': len(needle)} for addr in f.find(needle)]
    if a.action == 'read':
        return [{'address': a.address, 'hex': f.read(a.address, a.length).hex()}]
    if a.action == 'xref':
        if (a.text is None) == (a.address is None):
            raise ValueError('xref requires exactly one of TEXT or --address')
        addresses = [a.address] if a.address is not None else list(f.find(a.text.encode()))
        return [{'target': target, 'references': list(f.references(target))} for target in addresses]
    if a.action == 'callers':
        rows = []
        for addr, _, op, args, _ in f.instructions():
            if op not in (('jal', 'j', 'j8') if a.jumps else ('jal',)):
                continue
            match = re.match(r'(0x[0-9a-fA-F]+)(?:\s|$)', args)
            if match and number(match[1]) == a.address:
                rows.append({'address': addr, 'instruction': f'{op} {args}'})
        return rows
    fmt = ('>' if a.big_endian else '<') + dict(u8='B', u16='H', u32='I', i16='h', i32='i', pointer='I')[a.type]
    size = struct.calcsize(fmt)
    stride = a.stride if a.stride is not None else size
    if not 1 <= a.count <= 65536 or stride < size:
        raise ValueError('count must be 1..65536 and stride at least the element size')
    f.read(a.address, (a.count - 1) * stride + size)
    rows = []
    for i in range(a.count):
        addr = a.address + i * stride
        value = struct.unpack(fmt, f.read(addr, size))[0]
        row = {'index': i, 'address': addr, 'value': value}
        if a.strings:
            offset = value - a.base
            if 0 <= offset < len(f.data):
                raw = f.data[offset:offset + 256].split(b'\0', 1)[0]
                row['string'] = raw.decode('utf-8', errors='replace')
        rows.append(row)
    return rows


def main():
    a = parser().parse_args()
    try:
        rows = inspect(a)
        if a.json:
            print(json.dumps(rows, indent=2))
        else:
            for row in rows:
                if 'references' in row:
                    print(f'target={row["target"]:#x}')
                    for ref in row['references']:
                        print(f'  {ref["address"]:#x} {ref["kind"]}: {ref["instruction"]}')
                else:
                    print(' '.join(f'{key}={hex(value) if key in ("address", "target", "gp", "origin") else value}'
                                   for key, value in row.items()))
        return 0
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f'fw-inspect: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
