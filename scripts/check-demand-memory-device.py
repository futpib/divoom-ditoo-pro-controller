#!/usr/bin/env python3
"""Exercise on-demand memory on a physical Ditoo; stops the running Lua app.

Uses temporary apps only. Does not change the saved app, settings or bonds.
Run the ordinary hostile-script checks separately, then restore the user's app.
"""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--transport', choices=['ble', 'usb'], default='ble')
p.add_argument('--device')
p.add_argument('--usb-port')
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--firmware', type=int, choices=[306036, 306037], default=306036)
a = p.parse_args()
if a.transport == 'ble' and not a.device:
    p.error('BLE requires --device')
a.output.mkdir(parents=True, exist_ok=False)
base = [str(a.binary), '--transport', a.transport]
if a.device:
    base += ['--device', a.device]
if a.usb_port:
    base += ['--usb-port', a.usb_port]
preflight = subprocess.run(base+['firmware'], capture_output=True, text=True, check=True, timeout=45)
version = json.loads(preflight.stdout)['response']['firmware_versions'][0]
if version != a.firmware:
    raise SystemExit(f'Expected {a.firmware}, got {version}; no test commands sent')

requests, checks = [], []


def op(code, data=b'', delay=0):
    requests.append({'command': '0x37', 'payload_hex': (b'\x7fDLUA'+bytes([code])+data).hex(),
                     'response': '0x37', 'delay_ms': delay})


def status(name, state, kind='state'):
    checks.append((len(requests), name, state, kind))
    op(0)


PROBE = 'local s=device.stats();return s.free_heap..","..s.lua_used..","..s.lua_reserved'


def evaluate(source, name):
    source = source.encode()
    op(1, len(source).to_bytes(2, 'little')+source, 300)
    status(name, 2, 'heap' if source == PROBE.encode() else 'done')


def heap(name):
    evaluate(PROBE, name)


def start(source):
    source = source.encode()
    op(2, delay=300)
    op(3, len(source).to_bytes(2, 'little')+b'\x01')
    for offset in range(0, len(source), 512):
        op(4, offset.to_bytes(2, 'little')+source[offset:offset+512])
    op(5, delay=300)


op(2, delay=500)
heap('warmup')
heap('baseline')
for i in range(12):
    evaluate('return 42', f'worker-{i}-result')
    heap(f'worker-{i}-heap')
FRAME_APP = ('return {message=function(s) if s=="draw" then display.clear(0x010203);display.present() end '
             'local d=device.stats();app.log(d.free_heap..","..d.lua_used..","..d.lua_reserved) end}')
start(FRAME_APP)
op(8, b'heap', 200)
status('resident-without-frame', 4, 'resident-heap')
# Use identical fresh VMs: repeated telemetry creates Lua tables and can grow
# an arena page, whose native allocation overhead is not in lua_reserved.
start(FRAME_APP)
op(8, b'draw', 200)
status('resident-with-frame', 4, 'resident-heap')
op(6, delay=200)
status('paused-owns-vm', 5, 'resident')
op(7, delay=200)
status('resumed', 4, 'resident')
op(2, delay=500)
status('resident-stopped', 2, 'done')
heap('after-frame-stop')

# Start an upload without committing: expiry must work without a Lua worker.
op(3, (16384).to_bytes(2, 'little')+b'\x01')
op(4, b'\0\0return {}')
status('abandoned-upload', 6)
op(0, delay=30500)
status('upload-expired', 0, 'done')
heap('after-upload-expiry')

for _ in range(20):
    op(10, b'\0\0\0')
heap('after-storage-diagnostics')
op(13, b'\0\0\0\0\x01')
heap('trace-live')
op(13, b'\0\0\0\0\x00')
heap('after-trace-release')
op(13, b'\0\0\0\0\x01', 15500)
heap('after-trace-expiry')
op(2, delay=500)
status('final-stop', 2, 'done')
requests.append({'command': '0x37', 'payload_hex': '00', 'response': '0x37'})
request_file = a.output/'requests.json'
request_file.write_text(json.dumps(requests, indent=2)+'\n')
with (a.output/'replies.jsonl').open('w') as out, (a.output/'stderr.log').open('w') as err:
    run = subprocess.run(base+['raw', 'run', str(request_file)], stdout=out, stderr=err, timeout=300)
rows = [json.loads(s) for s in (a.output/'replies.jsonl').read_text().splitlines()]
results, heaps = [], {}
for index, name, state, kind in checks:
    d = bytes.fromhex(rows[index]['response']['data_hex']) if index < len(rows) else b''
    result = {'name': name, 'passed': len(d) >= 40 and d[:5] == b'DLUA\x02' and d[5] == state and d[6] == 0}
    if len(d) >= 40:
        result.update(state=d[5], result=d[40:].decode(errors='replace'),
                      lua_memory_bytes=int.from_bytes(d[16:20], 'little'))
        if kind in ('done', 'heap'):
            result['passed'] &= result['lua_memory_bytes'] == 0
        if kind in ('resident', 'resident-heap'):
            result['passed'] &= result['lua_memory_bytes'] > 0
        if kind in ('heap', 'resident-heap'):
            try:
                free, used, reserved = map(int, result['result'].split(','))
                result.update(free_native_heap=free, lua_used=used, lua_reserved=reserved,
                              free_plus_lua_reserved=free+reserved)
                result['passed'] &= free >= 24576 and reserved <= 49152
                heaps[name] = free+reserved
            except ValueError:
                result['passed'] = False
        if name.endswith('-result'):
            result['passed'] &= result['result'] == '42'
    results.append(result)


def compare(name, passed, **details):
    results.append(dict(name=name, passed=bool(passed), **details))


baseline = heaps.get('baseline')
for name in [*(f'worker-{i}-heap' for i in range(12)), 'after-frame-stop', 'after-upload-expiry',
             'after-storage-diagnostics', 'after-trace-release', 'after-trace-expiry']:
    compare(name+'-fully-reclaimed', baseline is not None and heaps.get(name) == baseline,
            baseline=baseline, observed=heaps.get(name))
if 'resident-without-frame' in heaps and 'resident-with-frame' in heaps:
    delta = heaps['resident-without-frame']-heaps['resident-with-frame']
    frame_samples = [r for r in results if r['name'] in ('resident-without-frame', 'resident-with-frame')]
    same_arena = len(frame_samples) == 2 and frame_samples[0]['lua_reserved'] == frame_samples[1]['lua_reserved']
    compare('frame-allocated-on-first-draw', same_arena and 768 <= delta <= 800,
            native_bytes=delta, same_lua_reservation=same_arena)
else:
    compare('frame-allocated-on-first-draw', False)
if baseline is not None and 'trace-live' in heaps:
    delta = baseline-heaps['trace-live']
    compare('trace-allocated-on-demand', 768 <= delta <= 800, native_bytes=delta)
else:
    compare('trace-allocated-on-demand', False)
healthy = (len(rows) == len(requests) and
           rows[-1]['response']['data_hex'] == (b'\x01'+version.to_bytes(4, 'little')).hex())
compare('stock-version-afterwards', healthy)
report = dict(firmware=version, transport=a.transport, exit_code=run.returncode, checks=results)
(a.output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
for result in results:
    print(result['name'], 'passed' if result['passed'] else 'FAILED', flush=True)
if run.returncode or not all(r['passed'] for r in results):
    raise SystemExit(1)
