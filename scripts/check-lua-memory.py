#!/usr/bin/env python3
"""Measure the TV app on hardware, then restore its unmodified running source.

Uses volatile uploads only. Saved apps and pairing records are untouched; the
remote may migrate its own settings to the current format.
The probe adds a message handler; compare runs using the same source and parameters.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--samples', type=int, default=60)
p.add_argument('--starts', type=int, default=5)
a = p.parse_args()
assert 1 <= a.samples <= 120 and 1 <= a.starts <= 20
a.output = a.output.resolve()
a.output.mkdir(parents=True, exist_ok=False)
base = [str(a.binary.resolve()), '--transport', 'usb']


def run(name, args):
    result = subprocess.run(base+args, text=True, capture_output=True, timeout=240)
    (a.output/(name+'.jsonl')).write_text(result.stdout)
    (a.output/(name+'.stderr')).write_text(result.stderr)
    result.check_returncode()
    return [json.loads(line) for line in result.stdout.splitlines()]


bundle = a.output/'tv.bundle.lua'
subprocess.run(base+['lua', 'bundle', str(ROOT/'examples/lua/tv-keyboard.lua'),
                    '--output', str(bundle)], check=True)
original = bundle.read_text()
anchor = 'message=function(s)'
assert original.count(anchor) == 1
probe = original.replace(anchor, anchor+
    "if s=='__heap' then local d=device.stats() "
    "comms.send(d.free_heap..','..d.lua_used..','..d.lua_reserved..','..d.lua_peak) return "
    "elseif s=='__saved' then comms.send(storage.get() or '') return end ")
assert len(probe.encode()) <= 16384
(a.output/'probe.lua').write_text(probe)
version_request = a.output/'version.json'
version_request.write_text(json.dumps([{'command':'0x37', 'payload_hex':'00', 'response':'0x37'}]))
reply = bytes.fromhex(run('version', ['raw', 'run', str(version_request)])[0]['response']['data_hex'])
assert len(reply) == 5 and reply[0] == 1
firmware = int.from_bytes(reply[1:], 'little')
steps = []
for _ in range(a.starts):
    steps += [{'op':'start', 'file':str(bundle)}, {'op':'sleep', 'ms':500},
              {'op':'wait', 'state':'active'}]
steps += [{'op':'start', 'file':'probe.lua'}, {'op':'wait', 'state':'active'},
          {'op':'send', 'message':'__saved'}, {'op':'sleep', 'ms':100}, {'op':'receive'}]
for _ in range(a.samples):
    steps += [{'op':'sleep', 'ms':1000}, {'op':'send', 'message':'__heap'},
              {'op':'sleep', 'ms':100}, {'op':'receive'}]
steps += [{'op':'send', 'message':'menu'}, {'op':'sleep', 'ms':200},
          {'op':'wait', 'state':'active', 'result_contains':'DEVICES: REMOTE'}]
sequence = a.output/'sequence.json'
sequence.write_text(json.dumps(steps, indent=2)+'\n')
try:
    rows = run('measurement', ['lua', 'sequence', str(sequence), '--timeout', '240'])
    messages = [r['status']['result'] for r in rows if r['op'] == 'receive']
    samples = [dict(zip(('free_native_heap', 'lua_used', 'lua_reserved', 'lua_peak'),
                       map(int, value.split(',')))) for value in messages[1:]]
    assert len(samples) == a.samples
    assert all(s['free_native_heap'] >= 24576 and s['lua_reserved'] <= 49152 for s in samples)
    report = {'firmware':firmware, 'bundle_bytes':len(original.encode()),
              'bundle_sha256':hashlib.sha256(original.encode()).hexdigest(),
              'probe_bytes':len(probe.encode()), 'starts_passed':a.starts,
              'startup_statuses':[r['status'] for r in rows if r['op']=='wait'][:a.starts],
              'settings':messages[0], 'samples':samples, 'menu_passed':True}
    (a.output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('settings','samples','startup_statuses')}))
    print(json.dumps({key:{'min':min(s[key] for s in samples), 'max':max(s[key] for s in samples)}
                      for key in samples[0]}))
finally:
    run('restored', ['lua', 'start', str(bundle)])
