#!/usr/bin/env python3
"""Build a no-global-state native memory probe for Lua placement research."""
import binascii
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('reflash', ROOT / 'scripts/build-reflash-probe.py')
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)


def build():
    stock = (ROOT / 'firmware/306007.MVA').read_bytes()
    assert hashlib.sha256(stock).hexdigest() == base.STOCK_SHA
    out = ROOT / 'target/lua'
    out.mkdir(parents=True, exist_ok=True)
    flags = ['-mcpu=d1088-spu', '-mabi=2', '-mno-fp-as-gp', '-Os', '-ffreestanding',
             '-fno-builtin', '-fno-stack-protector', '-ffunction-sections', '-fdata-sections']
    def run(*args):
        subprocess.run(args, cwd=ROOT, check=True)
    for source in ['probe.c', 'entry.S']:
        run('nds32le-elf-gcc', *flags, '-c', 'native/lua/' + source, '-o', str(out / (source + '.o')))
    run('nds32le-elf-gcc', '-mcpu=d1088-spu', '-nostdlib', '-Wl,-T,native/lua/probe.ld',
        '-o', str(out / 'probe.elf'), str(out / 'probe.c.o'), str(out / 'entry.S.o'))
    for section in ['hook', 'text']:
        run('nds32le-elf-objcopy', '-O', 'binary', '-j', '.' + section,
            str(out / 'probe.elf'), str(out / (section + '.bin')))
    hook, text = [(out / (s + '.bin')).read_bytes() for s in ['hook', 'text']]
    assert len(hook) <= 28 and 0x1ca000 + len(text) < 0x1d0000
    code = bytearray(stock[base.CODE:-4])
    assert code[0x3ab9c:0x3aba0] == bytes.fromhex('a639c805')
    code[0x3ab9c:0x3ab9c+len(hook)] = hook
    assert code[0x47924:0x47928] == bytes.fromhex('4404ab57')
    code[0x47924:0x47928] = bytes.fromhex('4404ab59')  # 306009
    assert code[0x4b550:0x4b552] == bytes.fromhex('c816')
    code[0x4b550:0x4b552] = bytes.fromhex('d516')
    code.extend(bytes(0x1ca000 - len(code)))
    code.extend(text)
    code.extend(bytes(-len(code) % 4))
    length = len(code)
    packed_length = length | ((sum(length.to_bytes(3, 'little')) & 255) << 24)
    struct.pack_into('<I', code, 0xd0, packed_length)
    marker = base.crc_code(code[0x10000:], len(code)-0x10000)
    assert marker != struct.unpack_from('<I', stock, base.CODE+0x100cc)[0]
    struct.pack_into('<I', code, 0x100cc, marker)
    struct.pack_into('<I', code, 0xcc, base.crc_code(code, len(code)))
    assert base.crc_code(code, 0x9e80) == 0x5f08
    image = bytearray(stock[:base.CODE])
    struct.pack_into('<I', image, 0x607, len(code)+4)
    image.extend(code)
    image.extend(struct.pack('<I', binascii.crc_hqx(image, 0)))
    report = {'version':306009, 'sha256':hashlib.sha256(image).hexdigest(),
              'bytes':len(image), 'code_bytes':len(code), 'probe_address':0x1ca000,
              'probe_bytes':len(text), 'boot_crc16':0x5f08,
              'status':'offline-built; hardware-unverified; not a Lua runtime'}
    (ROOT/'firmware/306009-lua-probe.MVA').write_bytes(image)
    (ROOT/'firmware/306009-lua-probe.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    build()
