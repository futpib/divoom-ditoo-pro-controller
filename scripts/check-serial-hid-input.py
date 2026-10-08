#!/usr/bin/env python3
"""Check real Linux input from the TV app while controlling it over Classic SPP.

Pair the Ditoo BLE remote to this Linux host first. Requires read access to its
input nodes. Temporarily changes the running app and its saved target; restores
settings and starts the ordinary TV app even after a failed check. Startup app
and bonds are not changed. EVIOCGRAB keeps test keys out of the desktop.
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
import threading

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--control', required=True, help='Ditoo Classic address')
p.add_argument('--hid', required=True, help='Ditoo BLE remote address')
p.add_argument('--host', required=True, help='This Linux adapter public address')
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
base = [str(a.binary.resolve()), '--transport', 'rfcomm', '--device', a.control, 'lua']
env = {**os.environ, 'RUST_LOG': 'warn'}
fds, observed, checks, capture_errors = [], [], [], []
event = struct.Struct('@llHHi')
stop = threading.Event()
settings = None
changed = False
active_process = None

def cli(*args):
    global active_process
    run = subprocess.Popen(base+list(args), cwd=ROOT, env=env, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, text=True)
    active_process = run
    try:
        out, err = run.communicate(timeout=180)
    except BaseException:
        run.kill()
        run.communicate()
        raise
    finally:
        active_process = None
    if run.returncode:
        raise RuntimeError((args, out, err))
    return [json.loads(line) for line in out.splitlines()]

def save_source(value):
    return ('local t,done;return {init=function() t=assert(storage.set('+json.dumps(value)+')) end,'
            'update=function() if not done then local ok,e=device.result(t);if ok~=nil then '
            'assert(ok,e);done=true;app.log("SAVED") end end end}')

def capture():
    while not stop.is_set():
        for fd in select.select(fds, [], [], .05)[0]:
            try:
                raw = os.read(fd, event.size*128)
            except OSError as error:
                capture_errors.append(str(error))
                process = active_process
                if process is not None:
                    process.terminate()  # Do not send test keys to a replacement, ungrabbed node.
                return
            for i in range(0, len(raw), event.size):
                _, _, kind, key, value = event.unpack_from(raw, i)
                if kind == 1 or (kind == 0 and key == 3):
                    observed.append([kind, key, value])

def key(steps, physical, expected=None, kind=1):
    steps += [{'op': 'send', 'message': f'K{physical},{kind}'}, {'op': 'sleep', 'ms': 350}]
    checks.append(dict(key=physical, event=kind,
                       linux_events=[] if expected is None else [[expected, 1], [expected, 0]]))

try:
    for node in Path('/sys/class/input').glob('event*'):
        identity = node/'device/uniq'
        if identity.exists() and identity.read_text().strip().lower() == a.hid.lower():
            fd = os.open('/dev/input/'+node.name, os.O_RDONLY|os.O_NONBLOCK)
            try:
                fcntl.ioctl(fd, 0x40044590, 1)
            except BaseException:
                os.close(fd)
                raise
            fds.append(fd)
    if not fds:
        raise RuntimeError('No real Ditoo Linux input nodes; nothing changed')
    with tempfile.TemporaryDirectory(prefix='ditoo-hid-input-') as directory:
        tmp = Path(directory)
        probe = cli('eval', "local b=keyboard.status();return (storage.get() or '')..';'..tostring(b.connected)..';'..(b.peer or '-')")[-1]
        settings, connected, peer = probe['result'].rsplit(';', 2)
        if connected != 'true' or peer.upper() != a.host.upper():
            raise RuntimeError(('Expected this Linux host as connected HID peer', probe))
        source = subprocess.check_output([base[0], 'lua', 'bundle', str(ROOT/'examples/lua/tv-keyboard.lua')], text=True)
        wrapper = ('local a=(function()\n'+source+'\nend)();local m=a.message;'
                   'a.message=function(s)local k,e=s:match("^K(%d+),(%d+)$");'
                   'if k then a.key(tonumber(k),tonumber(e)) else m(s) end end;return a')
        (tmp/'app.lua').write_text(wrapper)
        (tmp/'save.lua').write_text(save_source('TV6|'+a.host.upper()+'|public|1'))
        steps = [{'op':'start','file':str(tmp/'save.lua')}, {'op':'wait','result':'SAVED'},
                 {'op':'start','file':str(tmp/'app.lua')}, {'op':'wait','result_contains':'MEDIA:'},
                 {'op':'sleep','ms':1000}]
        for physical, code in [(4,164),(10,113),(1,115),(9,114),(2,105),(3,106)]:
            key(steps, physical, code)
        key(steps, 7)
        steps.append({'op':'wait','result_contains':'NAV:'})
        for kind in [2,3,4,5]:
            key(steps, 7, kind=kind)
        for physical, code in [(4,28),(10,57),(1,103),(9,108),(2,105),(3,106)]:
            key(steps, physical, code)
        key(steps, 0)
        steps.append({'op':'wait','result_contains':'DEVICES:'})
        for physical in [7,1,9,3,2,10]:
            key(steps, physical)
        steps.append({'op':'wait','result_contains':'NAV:'})
        key(steps, 10, 57)
        key(steps, 7)
        steps.append({'op':'wait','result_contains':'MEDIA:'})
        key(steps, 10, 113)
        steps.append({'op':'eval','source':"local b=keyboard.status();return b.state..';'..tostring(b.encrypted)..';'..(b.peer or '-')..';'..b.sent..';'..b.released..';'..b.errors"})
        (tmp/'sequence.json').write_text(json.dumps(steps))
        reader = threading.Thread(target=capture)
        reader.start()
        changed = True
        try:
            rows = cli('sequence', str(tmp/'sequence.json'), '--timeout', '120')
        finally:
            stop.set()
            reader.join()
        expected = [[1, *e] for check in checks for e in check['linux_events']]
        if capture_errors or observed != expected:
            raise RuntimeError(dict(expected=expected, observed=observed, capture_errors=capture_errors))
        state, encrypted, peer, sent, released, errors = rows[-1]['status']['result'].split(';')
        assert state == '2' and encrypted == 'true' and peer.upper() == a.host.upper()
        assert sent == released and errors == '0', rows[-1]
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(json.dumps(dict(checks=checks, observed=observed, status=rows[-1],
            control_transport='rfcomm', input_transport='ble', tv_tested=False,
            physical_buttons_tested=False), indent=2)+'\n')
        print(f'{len(checks)} physical-key callbacks checked against real Linux input over BLE')
except BaseException:
    print(json.dumps(dict(observed=observed)))
    raise
finally:
    try:
        if settings is not None:
            with tempfile.TemporaryDirectory(prefix='ditoo-hid-restore-') as directory:
                tmp = Path(directory)
                steps = []
                if changed:
                    (tmp/'save.lua').write_text(save_source(settings))
                    steps += [{'op':'start','file':str(tmp/'save.lua')}, {'op':'wait','result':'SAVED'}]
                steps.append({'op':'start','file':str(ROOT/'examples/lua/tv-keyboard.lua')})
                (tmp/'restore.json').write_text(json.dumps(steps))
                cli('sequence', str(tmp/'restore.json'))
    finally:
        for fd in fds:
            try:
                fcntl.ioctl(fd, 0x40044590, 0)
            except OSError:
                pass  # A disconnected input node has already released its grab.
            finally:
                os.close(fd)
