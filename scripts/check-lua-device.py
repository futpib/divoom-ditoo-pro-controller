#!/usr/bin/env python3
"""Check source execution and limits on an already-installed Lua runtime (no flash)."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('device')
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
p.add_argument('--kernel-disconnect', action='store_true', help='Use sudo -n btmgmt between checks for the observed BlueZ cleanup stall')
p.add_argument('--output', type=Path, required=True, help='New directory for device evidence')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
base = [str(a.binary), '--device', a.device, '--transport', 'ble']
checks = [
    ('string', "return 'Lua is on the Ditoo'", 0, 'done', 'Lua is on the Ditoo'),
    ('arithmetic', 'return 6*7', 0, 'done', '42'),
    ('float', 'return 7/2', 0, 'done', '3.5'),
    ('float-string', 'return tostring(7/2)', 0, 'done', '3.5'),
    ('integer-max', 'return 2147483647', 0, 'done', '2147483647'),
    ('second-program', 'local n=0; for i=1,100 do n=n+i end; return n', 0, 'done', '5050'),
    ('infinite-loop', 'while true do end', 1, 'error', 'instruction budget exceeded'),
    ('syntax-error', 'return function (', 1, 'error', None),
    ('memory-limit', 'local t={}; for i=1,10000 do t[i]={i,i,i,i} end', 1, 'error', None),
    ('recursion-limit', 'local function f() return 1+f() end; return f()', 1, 'error', None),
    ('max-source', 'return 123'.ljust(2048), 0, 'done', '123'),
    ('after-errors', 'return 99', 0, 'done', '99'),
]
results = []
for name, source, code, state, value in checks:
    run = subprocess.run([*base, 'lua', 'eval', source], capture_output=True, text=True, timeout=45)
    (a.output/(name+'.jsonl')).write_text(run.stdout)
    (a.output/(name+'.log')).write_text(run.stderr)
    if a.kernel_disconnect:
        cleanup = subprocess.run(['sudo','-n','btmgmt','--timeout','10','disconnect','-t','1',a.device],
                                 capture_output=True,text=True,timeout=15)
        (a.output/(name+'-disconnect.log')).write_text(cleanup.stdout+cleanup.stderr)
    replies = [json.loads(line) for line in run.stdout.splitlines()]
    reply = replies[-1] if replies else {}
    ok = run.returncode == code and reply.get('state') == state
    ok = ok and (value is None or reply.get('result') == value)
    results.append({'name':name, 'source':source, 'exit_code':run.returncode, 'passed':ok, 'result':reply})
    (a.output/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    print(name, 'passed' if ok else 'FAILED', flush=True)
    if not ok:
        raise SystemExit(f'Stopped after {name}; inspect {a.output}')
print('Different programs executed without firmware writes; limits and subsequent execution passed.')
