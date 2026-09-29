#!/usr/bin/env python3
"""Build the pinned NDS32 Lua runtime and its installable 306012 firmware."""
import binascii
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tarfile
from firmware_patches import PatchSet

PATCH_PROFILE = 'runtime'

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('reflash', ROOT/'scripts/build-reflash-probe.py')
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)


def build():
    out = ROOT/'target/lua/runtime'
    out.mkdir(parents=True, exist_ok=True)
    archive = ROOT/'native/lua/vendor/lua-5.4.9.tar.gz'
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == '2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6'
    with tarfile.open(archive) as tar:
        tar.extractall(out, filter='data')
    src = out/'lua-5.4.9/src'
    config = src/'luaconf.h'
    text = config.read_text()
    for before, after in [('#define LUA_32BITS\t0','#define LUA_32BITS\t1'),
                          ('#define LUAI_MAXSTACK\t\t1000000','#define LUAI_MAXSTACK\t\t512'),
                          ('#define LUAI_MAXSTACK\t\t15000','#define LUAI_MAXSTACK\t\t512')]:
        assert before in text
        text = text.replace(before, after)
    config.write_text(text)
    flags = ['-mcpu=d1088-spu','-mabi=2','-mno-fp-as-gp','-Os','-ffunction-sections',
             '-fdata-sections','-fno-stack-protector','-DLUAI_MAXCCALLS=20',
             '-Dluai_makeseed(L)=((unsigned)(L)^0x44554c41)', '-I'+str(src)]
    skip = {'lua.c','luac.c','linit.c','liolib.c','loslib.c','loadlib.c','ldblib.c'}
    sources = sorted(p for p in src.glob('*.c') if p.name not in skip)
    sources += [ROOT/'native/lua'/n for n in ['runtime.c','libc.c','runtime-entry.S']]
    patches = PatchSet(ROOT, PATCH_PROFILE, out)
    patch_object = patches.prepare()
    objects = []
    def run(*args): subprocess.run(args, cwd=ROOT, check=True)
    for source in sources:
        obj = out/(source.name+'.o')
        run('nds32le-elf-gcc', *flags, '-c', str(source), '-o', str(obj))
        objects.append(str(obj))
    run('nds32le-elf-gcc','-mcpu=d1088-spu','-nostartfiles',
        '-Wl,--gc-sections,--no-relax,-T,native/lua/runtime.ld,-Map,'+str(out/'runtime.map'),
        '-o',str(out/'runtime.elf'),*objects,patch_object,'-lm','-lc','-lgcc')
    run('nds32le-elf-size',str(out/'runtime.elf'))
    stock = (ROOT/'firmware/306007.MVA').read_bytes()
    code = patches.apply(stock, out/'runtime.elf')
    sections = {name: patches.extract('.'+name) for name in ('text','data')}
    for address, section in [(0x1ca000,'text'),(0x1ee000,'data')]:
        assert len(code) <= address
        code.extend(bytes(address-len(code)))
        code.extend(sections[section])
    code.extend(bytes(-len(code)%4))
    assert len(code) < 0x1f0000
    length = len(code)
    struct.pack_into('<I', code, 0xd0, length | ((sum(length.to_bytes(3,'little'))&255)<<24))
    marker = base.crc_code(code[0x10000:],length-0x10000)
    assert marker != 0xe05a
    struct.pack_into('<I', code, 0x100cc, marker)
    struct.pack_into('<I', code, 0xcc, base.crc_code(code,length))
    assert base.crc_code(code,0x9e80) == 0x5f08
    image = bytearray(stock[:base.CODE])
    struct.pack_into('<I', image,0x607,length+4)
    image.extend(code)
    image.extend(struct.pack('<I',binascii.crc_hqx(image,0)))
    patches.report(image)
    report = {'version':patches.version,'sha256':hashlib.sha256(image).hexdigest(),'bytes':len(image),
              'checksum':sum(image),'lua':'5.4.9','number_bits':32,'memory_limit':32768,
              'task_stack_words':4096,'globals_reserved':8192,'source_limit':2048,
              'instruction_limit':20000,'boot_crc16':0x5f08,
              'status':'offline-built; hardware-unverified'}
    (ROOT/f'firmware/{patches.version}-lua.MVA').write_bytes(image)
    (ROOT/f'firmware/{patches.version}-lua.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__ == '__main__': build()
