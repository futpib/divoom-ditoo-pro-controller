#!/usr/bin/env python3
"""Build the pinned NDS32 Lua runtime and its installable 306025 firmware."""
import binascii
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tarfile
from firmware_patches import PatchSet

PATCH_PROFILE = 'app'

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('reflash', ROOT/'scripts/build-reflash-probe.py')
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)


def build():
    out = ROOT/'target/lua-app/runtime'
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
    before = 'l_sprintf((s), sz, LUA_NUMBER_FMT, (LUAI_UACNUMBER)(n))'
    assert before in text
    text = text.replace(before, 'runtime_number((s), (sz), (n))')
    text = 'extern int runtime_number(char *, unsigned, float);\n' + text
    config.write_text(text)
    # The guard is outside Lua's protected-call mechanism and covers every coroutine.
    vm = src/'lvm.c'
    text = vm.read_text()
    anchor = '#define vmfetch()\t{ \\\n'
    assert anchor in text
    vm.write_text(text.replace(anchor, 'extern void runtime_poll(void);\n' + anchor + '  runtime_poll(); \\\n'))
    patterns = src/'lstrlib.c'
    text = patterns.read_text()
    anchor = '  init: /* using goto to optimize tail recursion */'
    assert anchor in text
    text = text.replace(anchor, '  init: runtime_poll(); /* includes pattern backtracking */')
    # Remove inaccessible functions from the registration tables so LTO can omit them.
    import re
    for name in ['format','dump','pack','unpack','packsize']:
        text, count = re.subn(r'  \{"'+name+r'",[^\n]+\n', '', text)
        assert count == 1
    patterns.write_text('extern void runtime_poll(void);\n' + text)
    base_lib = src/'lbaselib.c'
    text = base_lib.read_text()
    for name in ['dofile','loadfile','load','collectgarbage','print']:
        text, count = re.subn(r'  \{"'+name+r'",[^\n]+\n', '', text)
        assert count == 1
    base_lib.write_text(text)
    table_lib = src/'ltablib.c'
    table_lib.write_text('#define l_randomizePivot() (~0U)\n' + table_lib.read_text())
    math_lib = src/'lmathlib.c'
    text = math_lib.read_text()
    assert text.count('time(NULL)') == 1
    math_lib.write_text('extern unsigned stock_ticks(void);\n' + text.replace('time(NULL)', 'stock_ticks()'))


    flags = ['-mcpu=d1088-spu','-mabi=2','-mno-fp-as-gp','-Os','-ffunction-sections',
             '-flto','-fdata-sections','-fno-stack-protector','-DLUAI_MAXCCALLS=20',
             '-I/usr/nds32le-elf/include/newlib-nano','-Dluai_makeseed(L)=((unsigned)(L)^0x44554c41)', '-I'+str(src)]
    skip = {'lua.c','luac.c','linit.c','liolib.c','loslib.c','loadlib.c','ldblib.c'}
    sources = sorted(p for p in src.glob('*.c') if p.name not in skip)
    sources += [ROOT/'native/lua-app'/n for n in ['runtime.c','storage.c','number.c','usb-control.c','bluetooth-hid.c','bluetooth-trace.c','runtime-entry.S']]
    sources += [ROOT/'native/lua/libc.c']
    patches = PatchSet(ROOT, PATCH_PROFILE, out)
    patch_object = patches.prepare()
    objects = []
    def run(*args): subprocess.run(args, cwd=ROOT, check=True)
    for source in sources:
        obj = out/(source.name+'.o')
        run('nds32le-elf-gcc', *flags, '-c', str(source), '-o', str(obj))
        objects.append(str(obj))
    run('nds32le-elf-gcc','-mcpu=d1088-spu','-mabi=2','-mno-fp-as-gp','-nostartfiles','-flto','-Os',
        '-Wl,--gc-sections,--no-relax,-T,native/lua-app/runtime.ld,-Map,'+str(out/'runtime.map'),
        '-o',str(out/'runtime.elf'),*objects,patch_object,'-lm','-lc_nano','-lgcc')
    run('nds32le-elf-size',str(out/'runtime.elf'))
    symbols = subprocess.check_output(['nds32le-elf-nm',str(out/'runtime.elf')],text=True)
    for symbol in ['_gettimeofday','_gettimeofday_r','_times','_times_r','_open','_system']:
        assert not any(line.split()[-1] == symbol for line in symbols.splitlines()), symbol

    stock = (ROOT/'firmware/306007.MVA').read_bytes()
    # Check the stock SDP and L2CAP layouts used by keyboard-only mode.
    stock_code = stock[base.CODE:-4]
    for offset, expected in [
        (0x12eb7a,'c016fc01f0813c0ddd79'),
        (0x123faa,'3c1ddd795050a8fc5010a91ca2a9'),
        (0x12409e,'3c2ddd798060505128fc5021291c'),
        (0x123e34,'fc403c2ddd7984e04410007c'),
    ]:
        expected = bytes.fromhex(expected)
        assert stock_code[offset:offset+len(expected)] == expected
    code = patches.apply(stock, out/'runtime.elf')
    sections = {name: patches.extract('.'+name) for name in ('text','data')}
    for address, section in [(0x1ca000,'text'),(0x1ef800,'data')]:
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
              'checksum':sum(image),'lua':'5.4.9','number_bits':32,'memory_limit':49152,'arena_page_unit':1024,'arena_page_slots':48,
              'task_stack_words':4096,'globals_reserved':8192,'source_limit':8192,'source_chunk_bytes':512,
              'gc_pause_percent':120,'gc_step_multiplier':200,'gc_step_bytes':1024,
              'allocator_reclaims_shrunk_blocks':True,'allocator_grows_in_place':True,
              'instruction_limit':100000,'callback_ms_limit':50,'boot_crc16':0x5f08,
              'status':'offline-built; hardware-unverified'}
    (ROOT/f'firmware/{patches.version}-lua.MVA').write_bytes(image)
    (ROOT/f'firmware/{patches.version}-lua.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__ == '__main__': build()
