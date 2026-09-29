#!/usr/bin/env python3
"""Build a no-global-state native memory probe for Lua placement research."""
import binascii
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
from firmware_patches import PatchSet

PATCH_PROFILE = 'probe'

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('reflash', ROOT / 'scripts/build-reflash-probe.py')
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)


def build():
    stock = (ROOT / 'firmware/306007.MVA').read_bytes()
    assert hashlib.sha256(stock).hexdigest() == base.STOCK_SHA
    out = ROOT / 'target/lua'
    out.mkdir(parents=True, exist_ok=True)
    patches = PatchSet(ROOT, PATCH_PROFILE, out)
    patch_object = patches.prepare()
    flags = ['-mcpu=d1088-spu', '-mabi=2', '-mno-fp-as-gp', '-Os', '-ffreestanding',
             '-fno-builtin', '-fno-stack-protector', '-ffunction-sections', '-fdata-sections']
    def run(*args):
        subprocess.run(args, cwd=ROOT, check=True)
    for source in ['probe.c', 'entry.S']:
        run('nds32le-elf-gcc', *flags, '-c', 'native/lua/' + source, '-o', str(out / (source + '.o')))
    run('nds32le-elf-gcc', '-mcpu=d1088-spu', '-nostdlib', '-Wl,-T,native/lua/probe.ld',
        '-o', str(out / 'probe.elf'), str(out / 'probe.c.o'), str(out / 'entry.S.o'), patch_object)
    code = patches.apply(stock, out/'probe.elf')
    text = patches.extract('.text')
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
    patches.report(image)
    report = {'version':patches.version, 'sha256':hashlib.sha256(image).hexdigest(),
              'bytes':len(image), 'code_bytes':len(code), 'probe_address':0x1ca000,
              'probe_bytes':len(text), 'boot_crc16':0x5f08,
              'status':'offline-built; hardware-unverified; not a Lua runtime'}
    (ROOT/f'firmware/{patches.version}-lua-probe.MVA').write_bytes(image)
    (ROOT/f'firmware/{patches.version}-lua-probe.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    build()
