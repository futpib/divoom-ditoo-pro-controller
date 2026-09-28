#!/usr/bin/env python3
"""Exercise the resident runtime with its patched Lua VM under ASan/UBSan."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
source = ROOT/'target/lua-app/runtime/lua-5.4.9/src'
if not source.exists():
    raise SystemExit('Run scripts/build-lua-app-runtime.py first')
skip = {'lua.c','luac.c','linit.c','liolib.c','loslib.c','loadlib.c','ldblib.c'}
output = ROOT/'target/lua-app/runtime/test-runtime'
subprocess.run(['cc','-O1','-g','-fsanitize=address,undefined','-DLUAI_MAXCCALLS=20',
    '-Dluai_makeseed(L)=((unsigned long)(L)^0x44554c41)','-I'+str(source),
    str(ROOT/'native/lua-app/test-runtime.c'), str(ROOT/'native/lua-app/number.c'),
    *[str(p) for p in sorted(source.glob('*.c')) if p.name not in skip],
    '-lm','-o',str(output)],check=True)
subprocess.run([str(output)],check=True,timeout=30)
