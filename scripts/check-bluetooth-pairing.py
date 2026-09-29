#!/usr/bin/env python3
"""Exercise Lua's HID lifecycle against a real Linux Bluetooth host over USB.

Requires python-dbus, a working BlueZ HID host, and an agent such as
bluetooth-media-target.py accepting the selected Ditoo. Replaces the Lua app.
--fresh-pair explicitly removes only this laptop/Ditoo bond on both sides.
Other Ditoo bonds must survive unchanged. Never sends keyboard events.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

import dbus

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--device', required=True)
p.add_argument('--adapter', default='hci0')
p.add_argument('--fresh-pair', action='store_true')
p.add_argument('--outgoing-pair', action='store_true', help='with --fresh-pair, let Lua initiate pairing')
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
a = p.parse_args()
if a.outgoing_pair and not a.fresh_pair:
    p.error('--outgoing-pair requires --fresh-pair')
if not re.fullmatch(r'(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}', a.device):
    p.error('--device must be a Bluetooth address')
bus = dbus.SystemBus()
adapter_path = '/org/bluez/'+a.adapter
adapter = bus.get_object('org.bluez', adapter_path)
host = str(dbus.Interface(adapter, 'org.freedesktop.DBus.Properties').Get('org.bluez.Adapter1', 'Address'))
device_path = adapter_path+'/dev_'+a.device.upper().replace(':', '_')
hid_uuid = '00001124-0000-1000-8000-00805f9b34fb'
base = [str(a.binary), '--transport', 'usb', 'lua']


def bluez_device():
    return dbus.Interface(bus.get_object('org.bluez', device_path), 'org.bluez.Device1')


def cli(*args, allow_error=False):
    # Selecting native Bluetooth audio can re-enumerate USB after a cold boot.
    # Retry only read-only status queries; never resubmit a mutation.
    for attempt in range(10):
        r = subprocess.run(base+list(args), capture_output=True, text=True, timeout=15,
                           env={**os.environ, 'RUST_LOG': 'warn'})
        if r.stdout.strip() or args != ('status',) or not r.returncode:
            break
        time.sleep(.5)
    if not allow_error:
        assert r.returncode == 0, (args, r.stdout, r.stderr)
    return json.loads(r.stdout)


def log(name, **data):
    print(json.dumps(dict(check=name, passed=True, **data)), flush=True)


source = """local target='HOST'
local t,b={},{}
local pending
local function queue(name,seconds)
 pending=name;t=assert(keyboard[name](target,seconds))
end
return {
 init=function() pending='source';t=assert(audio.source('bluetooth')) end,
 update=function()
  if t then local ok,err=device.result(t)
   if ok~=nil then t=nil;app.log(ok and 'done:'..pending or 'error:'..err) end
  end
 end,
 message=function(s)
  if s=='status' then
   keyboard.status(b,true);local m=bluetooth.status()
   app.log(table.concat({b.state,b.error,b.opened,b.incoming,b.closed,
    tostring(b.paired),tostring(b.encrypted),tostring(b.pairing),
    b.access_mode,b.bonds_saved,b.forgotten,m.audio_state,m.media_state,
    tostring(m.ble_connected),b.pair_remaining_ms},','))
  elseif s=='bonds' then app.log(table.concat(keyboard.bonds(),','))
  elseif s=='crash' then while true do end
  elseif not t then
   if s:sub(1,5)=='pair:' then queue('pair',tonumber(s:sub(6)))
   else queue(s) end
  end
 end,
}
""".replace('HOST', host)


def start():
    with tempfile.NamedTemporaryFile(mode='w', suffix='.lua') as f:
        f.write(source);f.flush()
        cli('start', f.name)
    time.sleep(.25)
    assert cli('status')['result'] == 'done:source'


def message(s):
    cli('send', s)
    time.sleep(.15)
    return cli('status')['result']


def action(s):
    result = message(s)
    assert result == 'done:'+s.split(':')[0], (s, result)


names = ['state', 'error', 'opened', 'incoming', 'closed', 'paired', 'encrypted',
         'pairing', 'access_mode', 'bonds_saved', 'forgotten', 'audio', 'media', 'ble',
         'pair_remaining_ms']


def status():
    values = message('status').split(',')
    assert len(values) == len(names), values
    return dict(zip(names, [v == 'true' if v in ('true', 'false') else int(v) for v in values]))


def wait_for(predicate, seconds=45):
    end = time.monotonic()+seconds
    while True:
        s = status()
        if predicate(s):
            return s
        assert time.monotonic() < end, s
        time.sleep(.3)


def disconnect():
    action('disconnect')
    wait_for(lambda s: s['state'] == 0)
    try:
        bluez_device().Disconnect(timeout=15)
    except dbus.DBusException as e:
        if e.get_dbus_name() not in ('org.bluez.Error.NotConnected', 'org.freedesktop.DBus.Error.UnknownObject'):
            raise
    time.sleep(2)
    s = status()
    assert s['audio'] == s['media'] == 0 and not s['ble'], s
    return s


def connected():
    s = wait_for(lambda s: s['state'] == 2)
    assert s['paired'] and s['encrypted'] and not s['pairing'] and s['error'] == 0, s
    assert s['audio'] == s['media'] == 0 and not s['ble'], s
    return s


try:
    start()
    disconnect()
    if a.fresh_pair:
        before = set(filter(None, message('bonds').split(',')))
        previous = status()
        action('forget')
        after = set(filter(None, message('bonds').split(',')))
        assert after == before-{host}, (before, after)
        if host in before:
            wait_for(lambda s: s['bonds_saved'] > previous['bonds_saved'])
        dbus.Interface(adapter, 'org.bluez.Adapter1').RemoveDevice(device_path)
        log('targeted-forget', other_bonds_preserved=len(after))
        action('pair:120')
        assert status()['pairing']
        discovery = dbus.Interface(adapter, 'org.bluez.Adapter1')
        discovery.SetDiscoveryFilter(dbus.Dictionary({'Transport': 'bredr'}, signature='sv'))
        discovery.StartDiscovery()
        try:
            end = time.monotonic()+30
            while True:
                objects = dbus.Interface(bus.get_object('org.bluez', '/'),
                    'org.freedesktop.DBus.ObjectManager').GetManagedObjects()
                if device_path in objects:
                    break
                assert time.monotonic() < end, 'Ditoo not discovered'
                time.sleep(.25)
            if not a.outgoing_pair:
                bluez_device().Pair(timeout=55)
        finally:
            discovery.StopDiscovery()
            discovery.SetDiscoveryFilter(dbus.Dictionary({}, signature='sv'))
        if a.outgoing_pair:
            action('connect')
            attempts = 1
            # BlueZ can finish pairing before it has discovered the HID service
            # and close the first channels as an unknown input device. Match the
            # app's bounded retry, retaining the bond created by that exchange.
            for retry in range(3):
                state = wait_for(lambda s: s['state'] in (0, 2))
                if state['state'] == 2:
                    break
                time.sleep(15)
                action('connect')
                attempts += 1
        else:
            bluez_device().ConnectProfile(hid_uuid, timeout=55)
            attempts = 1
        s = connected()
        saved = wait_for(lambda s: s['bonds_saved'] > previous['bonds_saved']+int(host in before))
        log('fresh-outgoing-pair' if a.outgoing_pair else 'fresh-incoming-pair', status=saved,
            connection_attempts=attempts)
        disconnect()
        if a.outgoing_pair:
            time.sleep(10)  # Respect the native outgoing connection rate limit.
    action('listen')
    listening = status()
    assert listening['state'] == 4, listening
    bluez_device().ConnectProfile(hid_uuid, timeout=55)
    s = connected()
    assert s['incoming'] >= listening['incoming']+2, s
    log('bonded-incoming-connect', status=s)
    action('connect')
    reused = connected()
    assert reused['opened'] == s['opened'] and reused['closed'] == s['closed'], reused
    log('reuse-existing-link', status=reused)
    disconnect()
    action('connect')
    s = connected()
    assert s['incoming'] == reused['incoming'] and s['opened'] >= reused['opened']+2, s
    log('bonded-outgoing-connect', status=s)
    old = disconnect()
    action('pair:1')
    assert status()['pairing']
    expired = wait_for(lambda s: not s['pairing'], seconds=5)
    assert expired['access_mode'] == old['access_mode'] and expired['pair_remaining_ms'] == 0, expired
    log('pair-window-expiry', status=expired)
    disconnect()
    action('pair:120')
    assert status()['pairing']
    cli('send', 'crash', allow_error=True)
    time.sleep(.2)
    crashed = cli('status', allow_error=True)
    assert crashed['state'] == 'error' and 'budget' in crashed['result'], crashed
    start()
    stopped = wait_for(lambda s: not s['pairing'], seconds=5)
    assert stopped['access_mode'] == old['access_mode'], stopped
    log('pair-window-lua-crash', result=crashed['result'], status=stopped)
finally:
    # Stop owns no Bluetooth lifetime: explicitly disarm listening as well.
    try:
        start()
        disconnect()
    finally:
        cli('stop')
