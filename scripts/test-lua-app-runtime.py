#!/usr/bin/env python3
"""Exercise the resident runtime with 32-bit pointers and its patched Lua VM under ASan/UBSan."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
source = ROOT/'target/lua-app/runtime/lua-5.4.9/src'
if not source.exists():
    raise SystemExit('Run scripts/build-lua-app-runtime.py first')
# Use the same host bundler as uploads; the firmware still accepts one text chunk.
for name, entry in [('tv-keyboard','examples/lua/tv-keyboard.lua'),
                    ('ui-test','tests/lua/ui.lua'), ('bundle-test','tests/lua/bundle.lua'),
                    ('tree-test','tests/lua/tree.lua')]:
    subprocess.run(['cargo','run','--locked','--quiet','--no-default-features','--',
        'lua','bundle',entry,'--output',str(ROOT/'target/lua-app/runtime'/(name+'.bundle.lua'))],
        cwd=ROOT,check=True)
skip = {'lua.c','luac.c','linit.c','liolib.c','loslib.c','loadlib.c','ldblib.c'}
output = ROOT/'target/lua-app/runtime/test-runtime'
subprocess.run(['cc','-m32','-O1','-g','-fsanitize=address,undefined','-DLUAI_MAXCCALLS=20',
    '-Dluai_makeseed(L)=((unsigned long)(L)^0x44554c41)','-I'+str(source),
    str(ROOT/'native/lua-app/test-runtime.c'), str(ROOT/'native/lua-app/number.c'),
    *[str(p) for p in sorted(source.glob('*.c')) if p.name not in skip],
    '-lm','-o',str(output)],check=True)
subprocess.run([str(output)],check=True,timeout=30,cwd=ROOT)

usb_output = output.with_name('test-usb-control')
subprocess.run(['cc','-m32','-O1','-g','-fsanitize=address,undefined',
    str(ROOT/'native/lua-app/test-usb-control.c'),'-o',str(usb_output)],check=True)
subprocess.run([str(usb_output)],check=True,timeout=30,cwd=ROOT)

hid_output = output.with_name('test-bluetooth-hid')
subprocess.run(['cc','-m32','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
    str(ROOT/'native/lua-app/test-bluetooth-hid.c'),'-o',str(hid_output)],check=True)
subprocess.run([str(hid_output)],check=True,timeout=30,cwd=ROOT)
