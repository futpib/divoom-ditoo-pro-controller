#!/usr/bin/env python3
"""Exercise the standalone remote's real Lua callbacks against a Linux HID host.

Requires USB control, BlueZ, a pairing agent and access to the Ditoo event node.
Replaces the running app and its shared settings; forgets only the laptop bond.
The saved startup source is left untouched. Callback injection is not a physical
button test; native ADC dispatch is covered by the sanitizer tests.
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

import dbus

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--device', required=True)
p.add_argument('--after-reboot', action='store_true', help='read-only check of autostart and saved settings')
a = p.parse_args()
bus = dbus.SystemBus()
path = '/org/bluez/hci0/dev_'+a.device.upper().replace(':', '_')
adapter = bus.get_object('org.bluez', '/org/bluez/hci0')
properties = dbus.Interface(adapter, 'org.freedesktop.DBus.Properties')
host = str(properties.Get('org.bluez.Adapter1', 'Address'))
base = [str(ROOT/'target/release/divoom-ditoo-pro-controller'), '--transport', 'usb', 'lua']

def cli(*args):
    r = subprocess.run(base+list(args), capture_output=True, text=True, timeout=20)
    assert r.returncode == 0, (args, r.stdout, r.stderr)
    return json.loads(r.stdout)

def log(name, **data):
    print(json.dumps(dict(check=name, passed=True, **data)), flush=True)

def wait(predicate, seconds=30):
    end = time.monotonic()+seconds
    while True:
        s = cli('status')
        assert s['state'] == 'active', s
        if predicate(s):
            return s
        assert time.monotonic() < end, s
        time.sleep(.3)

def send(message):
    cli('send', message)
    time.sleep(.3)
    return cli('status')

def start(source):
    with tempfile.NamedTemporaryFile(mode='w', suffix='.lua') as f:
        f.write(source);f.flush();cli('start', f.name)

def probe():
    return send('probe')['result']

if a.after_reboot:
    # No upload, start, connect or settings mutation before this observation.
    s = wait(lambda s: s['result'].startswith('TV: READY'), 60)
    send('status')
    state = cli('status')['result']
    assert state == '2 '+host, state
    log('autostart-and-bonded-reconnect', status=s, peer=host)
    raise SystemExit(0)

try:
    dbus.Interface(bus.get_object('org.bluez', path), 'org.bluez.Device1').Disconnect(timeout=15)
except dbus.DBusException as e:
    assert e.get_dbus_name() in ('org.bluez.Error.NotConnected', 'org.freedesktop.DBus.Error.UnknownObject'), e
# Target the laptop so PAIR exercises forgetting precisely that native bond.
start("local t;return {init=function() t=assert(storage.set('TV4|"+host+"|')) end,"
      "update=function() local ok,e=device.result(t);if ok~=nil then assert(ok,e);app.log('saved') end end}")
wait(lambda s: s['result'] == 'saved')
source = subprocess.check_output([base[0], 'lua', 'bundle',
    str(ROOT/'examples/lua/tv-keyboard.lua')], text=True)
wrapper = "local a=(function()\n"+source+"\nend)();local m=a.message;a.message=function(s) local k=s:match('^K(%d+)$');if k then a.key(tonumber(k),1) elseif s=='probe' then local b=keyboard.status();app.log((storage.get() or '-')..' '..b.state..' '..tostring(b.pairing)..' '..b.error) else m(s) end end;return a"
assert len(wrapper.encode()) <= 8192
start(wrapper)
# Remove the host's stale service cache just as the TV's Forget action does.
try:
    dbus.Interface(adapter, 'org.bluez.Adapter1').RemoveDevice(path)
except dbus.DBusException as e:
    assert e.get_dbus_name() == 'org.bluez.Error.DoesNotExist', e
time.sleep(2)
assert probe().startswith('TV4|'+host+'| ')
log('fixed-controls-no-binding-setup')
send('K0');wait(lambda s: s['result'].startswith('LINK:'))
send('K3');wait(lambda s: s['result'].startswith('PAIR:'))
send('K4');wait(lambda s: 'RESET SAVED TV?' in s['result'])
send('K4');wait(lambda s: s['result'].startswith('PAIR:'))
assert ' 4 true 0' in probe(), probe()
log('physical-menu-opens-first-host-pairing')
discovery = dbus.Interface(adapter, 'org.bluez.Adapter1')
discovery.SetDiscoveryFilter(dbus.Dictionary({'Transport': 'bredr'}, signature='sv'))
discovery.StartDiscovery()
try:
    end = time.monotonic()+30
    while True:
        objects = dbus.Interface(bus.get_object('org.bluez', '/'), 'org.freedesktop.DBus.ObjectManager').GetManagedObjects()
        if path in objects:
            break
        assert time.monotonic() < end, 'Ditoo not discovered'
        time.sleep(.2)
    peer = dbus.Interface(bus.get_object('org.bluez', path), 'org.bluez.Device1')
    peer.Pair(timeout=55)
    peer.ConnectProfile('00001124-0000-1000-8000-00805f9b34fb', timeout=55)
finally:
    discovery.StopDiscovery()
    discovery.SetDiscoveryFilter(dbus.Dictionary({}, signature='sv'))
wait(lambda s: s['result'].startswith('TV: READY'))
time.sleep(3)
assert probe() == 'TV4|'+host+'| 2 false 0'
log('fresh-wildcard-pair-and-learned-peer')
nodes = [Path('/dev/input')/e.name for e in Path('/sys/class/input').glob('event*')
         if (e/'device/uniq').exists() and (e/'device/uniq').read_text().strip().lower() == a.device.lower()]
assert len(nodes) == 1, nodes
fd = os.open(nodes[0], os.O_RDONLY|os.O_NONBLOCK)
fmt = struct.Struct('@llHHi')
try:
    fcntl.ioctl(fd, 0x40044590, 1)
    for key, code in ((4,164),(10,113),(7,57),(1,115),(9,114),(2,105),(3,106)):
        send('K'+str(key));events=[];end=time.monotonic()+3
        while time.monotonic() < end:
            if select.select([fd], [], [], .1)[0]:
                raw=os.read(fd, fmt.size*128)
                for offset in range(0, len(raw), fmt.size):
                    _, _, kind, got, value=fmt.unpack_from(raw, offset)
                    if kind == 1: events.append([got,value])
            if events == [[code,1],[code,0]]: break
        assert events == [[code,1],[code,0]], events
        log('button-'+str(key), events=events)
finally:
    fcntl.ioctl(fd, 0x40044590, 0);os.close(fd)
# M opens LINK; two Right presses browse to OFF.
send('K0')
for _ in range(2): send('K3')
send('K4');wait(lambda s: 'OFFLINE' in s['result'])
time.sleep(2)
assert probe().endswith(' 0 false 0'), probe()
send('K0');send('K4');wait(lambda s: s['result'].startswith('TV: READY'), 60)
log('menu-disconnect-and-reconnect')
cli('start', str(ROOT/'examples/lua/tv-keyboard.lua'))
wait(lambda s: s['result'].startswith('TV: READY'))
log('original-app-reloads-saved-peer')
