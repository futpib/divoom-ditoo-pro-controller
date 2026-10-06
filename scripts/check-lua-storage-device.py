#!/usr/bin/env python3
"""Verify 16 KiB saved apps on 306030+ using USB or BLE.

Replaces the running and saved app with three full-size probes and small
probes that exercise journal compaction, then installs --restore when supplied.
Preserves settings and Bluetooth bonds. The probes
send no keyboard input. Raw responses remain in the supplied output directory.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--transport', choices=['usb', 'ble'], default='usb')
p.add_argument('--device')
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--restore', type=Path)
a = p.parse_args()
if a.transport == 'ble' and not a.device:
    p.error('BLE requires --device')
a.output.mkdir(parents=True, exist_ok=False)
base = [str(a.binary), '--transport', a.transport]
if a.device:
    base += ['--device', a.device]
checks = []


def command(*args):
    r = subprocess.run(base+list(args), capture_output=True, text=True,
                       env={**os.environ, 'RUST_LOG': 'warn'}, timeout=300)
    if args[:2] == ('raw', 'run'):
        path = Path(args[2])
        path.with_suffix('.replies.jsonl').write_text(r.stdout)
        path.with_suffix('.stderr').write_text(r.stderr)
    assert r.returncode == 0, r.stderr or r.stdout
    return [json.loads(s) for s in r.stdout.splitlines()]


def check(name, **fields):
    row = dict(check=name, passed=True, **fields)
    checks.append(row)
    print(json.dumps(row), flush=True)


def install_probe(source, name):
    # Exercise the exact wire size independently of bundle optimizations.
    requests = []
    def op(code, data=b'', delay=0):
        requests.append(dict(command='0x37', response='0x37', delay_ms=delay,
            payload_hex=(b'\x7fDLUA'+bytes([code])+data).hex()))
    op(2, delay=250)
    op(3, len(source).to_bytes(2, 'little')+b'\x02')
    for offset in range(0, len(source), 512):
        op(4, offset.to_bytes(2, 'little')+source[offset:offset+512])
    op(5, delay=1000)
    op(0)
    path = a.output/(name+'-requests.json')
    path.write_text(json.dumps(requests)+'\n')
    rows = command('raw', 'run', str(path))
    assert len(rows) == len(requests)
    for row in rows:
        data = bytes.fromhex(row['response']['data_hex'])
        assert len(data) >= 40 and data[:5] == b'DLUA\x02' and data[6] == 0, data.hex()
    return command('lua', 'status')[-1]


def reload_saved():
    requests = []
    for action, key in [(1, 0), (2, 2), (2, 2), (2, 4)]:
        for op, button in [(action, key), (0, 0)]:
            requests.append(dict(command='0x37', response='0x37', delay_ms=500,
                payload_hex=(b'\x7fDLUA\x0e'+bytes([op, button, 0])).hex()))
    path = a.output/'reload-requests.json'
    path.write_text(json.dumps(requests)+'\n')
    rows = command('raw', 'run', str(path))
    payloads = [bytes.fromhex(r['response']['data_hex']) for r in rows]
    assert all(len(r) == 160 and r[:6] == b'DMNU\1\0' for r in payloads)
    assert payloads[5][8] == 30, 'Saved app must be selected before launching'


def journal_used():
    rows = command('raw', 'send', '0x37', '--data', '7f444c55410c000000', '--query')
    data = bytes.fromhex(rows[0]['response']['data_hex'])
    assert len(data) == 48 and data[:6] == b'DCFG\1\0', data[:16].hex()
    return int.from_bytes(data[20:24], 'little')


version = command('raw', 'send', '0x37', '--data', '00', '--query')[0]['response']['firmware_versions'][0]
assert version >= 306030, '16 KiB persistence requires 306030+'
for generation in ['A', 'B', 'C']:
    source = ("return {init=function() app.log('16K "+generation+"') end}\n--").encode()
    source += b'x'*(16384-len(source))
    path = a.output/('probe-'+generation+'.lua')
    path.write_bytes(source)
    status = install_probe(source, 'probe-'+generation)
    assert status['state'] == 'active' and status['result'] == '16K '+generation, status
    check('full-size-save-'+generation, source_bytes=len(source),
          sha256=hashlib.sha256(source).hexdigest(), status=status)
    reload_saved()
    reloaded = command('lua', 'status')[-1]
    assert reloaded['state'] == 'active' and reloaded['result'] == '16K '+generation, reloaded
    assert reloaded['generation'] > status['generation'], (status, reloaded)
    check('full-size-native-menu-reload-'+generation, status=reloaded)
for attempt in range(16):
    before = journal_used()
    source = ("return {init=function() app.log('GC "+str(attempt)+"') end}\n--").encode()
    source += b'x'*(4096-len(source))
    path = a.output/'compaction-probe.lua'
    path.write_bytes(source)
    status = install_probe(source, 'compaction-probe')
    assert status['state'] == 'active' and status['result'] == 'GC '+str(attempt), status
    after = journal_used()
    if after < before:
        reload_saved()
        status = command('lua', 'status')[-1]
        assert status['state'] == 'active' and status['result'] == 'GC '+str(attempt), status
        check('native-journal-compaction-and-reload', before_pages=before, after_pages=after)
        break
else:
    raise AssertionError('No journal compaction observed after sixteen changed saves')
if a.restore:
    status = command('lua', 'install', str(a.restore))[-1]
    assert status['state'] == 'active', status
    check('restored-saved-app', status=status)
(a.output/'verification.json').write_text(json.dumps(dict(firmware=version, checks=checks), indent=2)+'\n')
