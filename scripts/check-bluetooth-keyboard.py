#!/usr/bin/env python3
"""Verify a connected Ditoo HID keyboard through real Linux input events.

Requires read access to its event node (usually sudo). Grabs only the HID device
whose Bluetooth address matches --device, so test keys cannot reach the desktop.
Uses USB control, replaces the resident Lua app, then restores tv-keyboard.lua.
"""
import argparse
import fcntl
import json
import os
from pathlib import Path
import select
import struct
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--device', required=True)
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
p.add_argument('--guard', action='store_true', help='also abort an infinite Lua loop after a key press')
a = p.parse_args()
nodes = []
for e in Path('/sys/class/input').glob('event*'):
    uniq = e/'device/uniq'
    if uniq.exists() and uniq.read_text().strip().lower() == a.device.lower():
        nodes.append(Path('/dev/input')/e.name)
if len(nodes) != 1:
    raise SystemExit(f'Expected exactly one Ditoo HID event node, found {nodes}')
base = [str(a.binary), '--transport', 'usb', 'lua']
def command(*args, allow_error=False):
    r = subprocess.run(base+list(args), capture_output=True, text=True,
                       env={**os.environ, 'RUST_LOG':'warn'}, timeout=15)
    if not allow_error:
        r.check_returncode()
    return json.loads(r.stdout) if r.stdout.strip() else {'stderr':r.stderr}

fd = os.open(nodes[0], os.O_RDONLY|os.O_NONBLOCK)
fmt = struct.Struct('@llHHi')
checks = []
def receive(expected, seconds=3):
    events = []
    end = time.monotonic()+seconds
    while time.monotonic() < end:
        if select.select([fd], [], [], min(0.1,end-time.monotonic()))[0]:
            raw = os.read(fd,fmt.size*128)
            for offset in range(0,len(raw),fmt.size):
                _,_,kind,code,value = fmt.unpack_from(raw,offset)
                if kind == 1:
                    events.append([code,value])
        if events == [[expected,1],[expected,0]]:
            return events
    raise AssertionError(f'Expected press/release of {expected}, got {events}')

try:
    fcntl.ioctl(fd,0x40044590,1)  # EVIOCGRAB
    command('start',str(ROOT/'examples/lua/tv-keyboard.lua'))
    for action,code in [('toggle',164),('mute',113),('space',57)]:
        command('send',action)
        result = {'check':action,'events':receive(code),'passed':True}
        checks.append(result);print(json.dumps(result),flush=True)
        time.sleep(0.35)
    if a.guard:
        source = '''local before=keyboard.status().sent
return {init=function() assert(keyboard.tap(44)) end,
update=function() if keyboard.status().sent>before then while true do end end end}
'''
        with tempfile.NamedTemporaryFile(mode='w',suffix='.lua') as script:
            script.write(source);script.flush()
            command('start',script.name,allow_error=True)
        events = receive(57)
        status = command('status',allow_error=True)
        assert status['state']=='error',status
        assert any(word in status['result'] for word in ('budget','deadline','limit')),status
        result = {'check':'infinite-loop-release','events':events,'state':status['state'],
                  'result':status['result'],'passed':True}
        checks.append(result);print(json.dumps(result),flush=True)
finally:
    try:
        command('start',str(ROOT/'examples/lua/tv-keyboard.lua'))
    finally:
        fcntl.ioctl(fd,0x40044590,0);os.close(fd)
