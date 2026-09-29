#!/usr/bin/env python3
"""Build the pinned NDS32 Lua runtime and its installable 306021 firmware."""
import binascii
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tarfile

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
    sources += [ROOT/'native/lua-app'/n for n in ['runtime.c','storage.c','number.c','usb-control.c','bluetooth-hid.c','runtime-entry.S']]
    sources += [ROOT/'native/lua/libc.c']
    objects = []
    def run(*args): subprocess.run(args, cwd=ROOT, check=True)
    for source in sources:
        obj = out/(source.name+'.o')
        run('nds32le-elf-gcc', *flags, '-c', str(source), '-o', str(obj))
        objects.append(str(obj))
    run('nds32le-elf-gcc','-mcpu=d1088-spu','-mabi=2','-mno-fp-as-gp','-nostartfiles','-flto','-Os',
        '-Wl,--gc-sections,--no-relax,-T,native/lua-app/runtime.ld,-Map,'+str(out/'runtime.map'),
        '-o',str(out/'runtime.elf'),*objects,'-lm','-lc_nano','-lgcc')
    run('nds32le-elf-size',str(out/'runtime.elf'))
    symbols = subprocess.check_output(['nds32le-elf-nm',str(out/'runtime.elf')],text=True)
    for symbol in ['_gettimeofday','_gettimeofday_r','_times','_times_r','_open','_system']:
        assert not any(line.split()[-1] == symbol for line in symbols.splitlines()), symbol

    sections = {}
    for section in ['hook','init_hook','screen_hook','key_hook','led_hook','native_hook','indicator_hook','noise_hook','wake_select_hook','bt_command_hook','usb_receive_hook','usb_send_hook','usb_response_hook','usb_connected_hook','text','data']:
        path = out/(section+'.bin')
        run('nds32le-elf-objcopy','-O','binary','-j','.'+section,str(out/'runtime.elf'),str(path))
        sections[section] = path.read_bytes()
    stock = (ROOT/'firmware/306007.MVA').read_bytes()
    assert hashlib.sha256(stock).hexdigest() == base.STOCK_SHA
    code = bytearray(stock[base.CODE:-4])
    # Match the native queue wrappers used by the bounded Bluetooth API.
    for offset, expected in [
        (0x138c7a,'f0034e0204bb8e015cf0004a4ef204b4'),
        (0x1395ee,'49fffb27fcc6'),
        (0x11c52e,'c076fc20007002bb66508080001002ba'),
        (0x1139b0,'fc01f081f1018446fa02490128ddfc81'),
        (0x1139c0,'fc0084208041fa03490128d6fc80'),
        (0x1139ce,'3c0de560000000a6c008fc0084208041fa04490128cafc80dd9e'),
        (0x1139e8,'3c0de560000000a6c008fc0084208041fa05490128bdfc80dd9e')]:
        expected = bytes.fromhex(expected)
        assert code[offset:offset+len(expected)] == expected
    patches = [(0x387b2, bytes.fromhex('4900044b'), sections['usb_connected_hook']),
               (0x7a27e, bytes.fromhex('49fd87e1'), sections['usb_receive_hook']),
               (0x7a29c, bytes.fromhex('49008a1d'), sections['usb_send_hook']),
               (0x38f00, bytes.fromhex('3a6fb0bc'), sections['usb_response_hook']),
               (0x3ab9c, bytes.fromhex('a639c805'), sections['hook']),
               (0x2ec58, bytes.fromhex('4902b436'), sections['init_hook']),
               (0x854d0, bytes.fromhex('4602004c'), bytes.fromhex('4602004a')),
               (0x75dcc, bytes.fromhex('3a6f98bc'), sections['screen_hook']),
               (0x2d490, bytes.fromhex('49fff590'), sections['key_hook']),
               (0x7580c, bytes.fromhex('3bfffcbc'), sections['led_hook']),
               (0x47838, bytes.fromhex('49001752'), sections['native_hook']),
               (0x138c76, bytes.fromhex('49ffffc0'), sections['bt_command_hook']),
               (0x2d6e8, bytes.fromhex('3a6f98bc'), sections['indicator_hook']),
               (0x72192, bytes.fromhex('49ff14eb'), sections['noise_hook']),
               # Native one-shot cleanup must retain all nine 16-byte schedules.
               (0x47fdc, bytes.fromhex('44300048'), bytes.fromhex('44300090')),
               (0x4818e, bytes.fromhex('44300048'), bytes.fromhex('44300090')),
               # Remember the selected earliest slot, not the last eligible slot.
               (0x47eba, bytes.fromhex('3e177b1c'), bytes.fromhex('92009200')),
               (0x47eea, bytes.fromhex('3c0fdbd1'), sections['wake_select_hook']),
               (0x47924, bytes.fromhex('4404ab57'), bytes.fromhex('4404ab65')),
               (0x4b550, bytes.fromhex('c816'), bytes.fromhex('d516'))]
    for offset, before, after in patches:
        assert code[offset:offset+len(before)] == before
        code[offset:offset+len(after)] = after
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
    report = {'version':306021,'sha256':hashlib.sha256(image).hexdigest(),'bytes':len(image),
              'checksum':sum(image),'lua':'5.4.9','number_bits':32,'memory_limit':49152,'arena_page_unit':1024,'arena_page_slots':48,
              'task_stack_words':4096,'globals_reserved':8192,'source_limit':8192,
              'instruction_limit':100000,'callback_ms_limit':50,'boot_crc16':0x5f08,
              'status':'offline-built; hardware-unverified'}
    (ROOT/'firmware/306021-lua.MVA').write_bytes(image)
    (ROOT/'firmware/306021-lua.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__ == '__main__': build()
