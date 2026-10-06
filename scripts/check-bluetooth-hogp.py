#!/usr/bin/env python3
"""Test the Ditoo BLE remote against this Linux laptop, never a TV.

Requires root for EVIOCGRAB, python-dbus, PyGObject, and 306029+ on the Ditoo.
The Ditoo must already be bonded to this laptop. The running app must select
ble-remote and target this laptop. Replaces
that app during input tests; restores tv-keyboard.lua with the existing saved
laptop target. Does not erase bonds or change any other Bluetooth device.
"""
import argparse
import fcntl
import json
import os
from pathlib import Path
import select
import struct
import subprocess
import sys
import tempfile
import time

import dbus
import dbus.mainloop.glib
from gi.repository import GLib

ROOT = Path(__file__).resolve().parents[1]
PROPS = 'org.freedesktop.DBus.Properties'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--device', required=True)
p.add_argument('--adapter', default='hci0')
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
a = p.parse_args()
if os.geteuid() != 0:
    raise SystemExit('Run with sudo to grab only the matching Ditoo input nodes')
dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SystemBus()
path = '/org/bluez/'+a.adapter
peer_path = path+'/dev_'+a.device.upper().replace(':','_')
obj = bus.get_object('org.bluez', peer_path)
props = dbus.Interface(obj, PROPS)
le = dbus.Interface(obj, 'org.bluez.Bearer.LE1')
adapter = dbus.Interface(bus.get_object('org.bluez', path), PROPS)
laptop = str(adapter.Get('org.bluez.Adapter1', 'Address')).upper()
base = [str(a.binary),'--transport','ble','--device',a.device,'lua']
fds = []
fmt = struct.Struct('@llHHi')
checks = []

def command(*args, allow_error=False):
    r = subprocess.run(base+list(args), capture_output=True, text=True,
                       env={**os.environ,'RUST_LOG':'warn'}, timeout=45)
    if r.returncode and not allow_error:
        raise RuntimeError(r.stderr or r.stdout)
    rows = [json.loads(s) for s in r.stdout.splitlines() if s.startswith('{')]
    return rows[-1] if rows else {'stderr':r.stderr}

def script(source, allow_error=False):
    with tempfile.NamedTemporaryFile('w',suffix='.lua') as f:
        f.write(source);f.flush()
        return command('start',f.name,allow_error=allow_error)

def check(name, **fields):
    row = dict(check=name,passed=True,**fields)
    checks.append(row);print(json.dumps(row),flush=True)

def spin(seconds):
    end = time.monotonic()+seconds
    while time.monotonic()<end:
        while GLib.MainContext.default().pending():
            GLib.MainContext.default().iteration(False)
        time.sleep(.02)

def connect():
    try: le.Connect(timeout=30)
    except dbus.exceptions.DBusException as e:
        if e.get_dbus_name() != 'org.bluez.Error.AlreadyConnected': raise

def grab():
    end=time.monotonic()+15
    while time.monotonic()<end:
        nodes=[]
        for e in Path('/sys/class/input').glob('event*'):
            uniq=e/'device/uniq'
            if uniq.exists() and uniq.read_text().strip().lower()==a.device.lower():
                nodes.append(Path('/dev/input')/e.name)
        if nodes:
            for node in nodes:
                fd=os.open(node,os.O_RDONLY|os.O_NONBLOCK)
                try: fcntl.ioctl(fd,0x40044590,1)
                except BaseException:
                    os.close(fd);raise
                fds.append(fd)
            return
        spin(.1)
    raise RuntimeError('No real Linux HID input device appeared')

def ungrab():
    while fds:
        fd=fds.pop()
        try: fcntl.ioctl(fd,0x40044590,0)
        except OSError: pass # The peer may already have disconnected.
        os.close(fd)

def receive(code):
    events=[];end=time.monotonic()+5
    while time.monotonic()<end:
        for fd in select.select(fds,[],[],.1)[0]:
            raw=os.read(fd,fmt.size*128)
            for i in range(0,len(raw),fmt.size):
                _,_,kind,key,value=fmt.unpack_from(raw,i)
                if kind==1: events.append([key,value])
        if events==[[code,1],[code,0]]: return events
    raise AssertionError(f'Expected press/release {code}, received {events}')

INPUT_APP = '''local b={}
return {init=function()
 assert(storage.get()=='TV5|LAPTOP|public|','saved target must be this laptop')
 keyboard.status(b)
 assert(b.transport=='ble' and (not b.connected or b.peer=='LAPTOP'),'BLE peer must be this laptop')
 if not b.connected then assert(keyboard.listen('LAPTOP')) end
end,message=function(s)
 if s=='status' then keyboard.status(b);app.log((b.peer or '-')..' '..tostring(b.connected)..' '..(b.transport or '-'))
 elseif s:sub(1,1)=='k' then assert(keyboard.tap(tonumber(s:sub(2))))
 else assert(keyboard.media(s)) end
end}'''.replace('LAPTOP',laptop)
safe_to_restore=False
try:
    assert bool(props.Get('org.bluez.Bearer.LE1','Bonded')),'Pair this laptop first'
    # Explicitly select LE; generic Connect could select a Classic audio bearer.
    # Discard an ATT session left over from a firmware restart before uploading.
    if bool(props.Get('org.bluez.Bearer.LE1','Connected')):
        le.Disconnect(timeout=15);spin(1)
    connect()
    script(INPUT_APP)
    spin(.5);le.Disconnect(timeout=15);spin(1);connect()
    grab()
    script(INPUT_APP);command('send','status');spin(.2)
    status=command('status')
    assert status['result']==laptop+' true ble',status
    safe_to_restore=True
    check('peer-is-this-laptop')
    assert bool(props.Get('org.bluez.Bearer.LE1','Bonded'))
    check('bonded-host',new_pair_requested=False)
    supported=set()
    for fd in fds:
        bits=bytearray(128);fcntl.ioctl(fd,0x80804521,bits,True)
        supported.update(i for i in range(1024) if bits[i//8] & (1<<(i%8)))
    expected={1,28,57,103,105,106,108,113,114,115,163,164,165,166}
    assert supported==expected,supported
    check('restricted-remote-descriptor',linux_keys=sorted(supported))
    for action,code in [('k40',28),('k41',1),('k44',57),('k79',106),('k80',105),('k81',108),('k82',103),
                        ('play_pause',164),('mute',113),('volume_up',115),('volume_down',114),
                        ('next',163),('previous',165),('stop',166)]:
        command('send',action);check(action,events=receive(code))
    script('''local before=keyboard.status().sent
return {init=function() assert(keyboard.tap(44)) end,
update=function() if keyboard.status().sent>before then while true do end end end}''',allow_error=True)
    events=receive(57);status=command('status',allow_error=True)
    assert status['state']=='error' and any(w in status['result'] for w in ('budget','deadline','limit')),status
    check('infinite-loop-release',events=events)
    script(INPUT_APP)
    ungrab();le.Disconnect(timeout=15);spin(1);connect()
    grab();spin(1);command('send','status');spin(.2)
    status=command('status');assert status['result']==laptop+' true ble',status
    command('send','play_pause');check('bonded-reconnect',events=receive(164))
    source=subprocess.check_output([str(a.binary),'lua','bundle',str(ROOT/'examples/lua/tv-keyboard.lua')],text=True)
    wrapper="local a=(function()\n"+source+"\nend)();local m=a.message;a.message=function(s) local k=s:match('^K(%d+)$');if k then a.key(tonumber(k),1) else m(s) end end;return a"
    assert len(wrapper.encode())<=8192
    script(wrapper)
    spin(3)
    for key,code in [(4,164),(10,113),(7,57),(1,115),(9,114),(2,105),(3,106)]:
        command('send','K'+str(key));check('remote-app-button-'+str(key),events=receive(code))
    # Open native Saved devices and cancel its default-No confirmation.
    steps=[(1,0,0,0),(2,2,0,0),(2,4,1,0),(2,3,1,1),
           (2,3,1,2),(2,3,1,3),(2,4,5,0),(2,4,11,0),(2,0,5,0)]
    requests=[]
    for action,key,_,_ in steps:
        for op,button in [(action,key),(0,0)]:
            requests.append(dict(command='0x37',payload_hex=(b'\x7fDLUA\x0e'+bytes([op,button,0])).hex(),
                                 response='0x37',delay_ms=500))
    with tempfile.NamedTemporaryFile('w',suffix='.json') as f:
        json.dump(requests,f);f.flush()
        r=subprocess.run(base[:-1]+['raw','run',f.name],capture_output=True,text=True,timeout=45)
        assert r.returncode==0,r.stderr
    rows=[bytes.fromhex(json.loads(s)['response']['data_hex']) for s in r.stdout.splitlines()]
    assert len(rows)==len(requests)
    assert all(len(r)==160 and r[:6]==b'DMNU\x01\x00' for r in rows)
    for (_,_,page,index),r in zip(steps,rows[1::2]):
        assert (r[6],r[7])==(page,index),(page,index,list(r[:16]))
    assert rows[13][9]>0 and rows[13][9]==rows[17][9]
    check('native-ble-bonds-and-cancel',bond_count=rows[13][9])
    check('complete',checks=len(checks))
finally:
    failed=sys.exc_info()[0] is not None
    restore_error=None
    if safe_to_restore:
        try:
            command('start',str(ROOT/'examples/lua/tv-keyboard.lua'))
            spin(1)
        except Exception as e:
            restore_error=e
            print(f'Remote app restoration failed: {e}',file=sys.stderr)
    ungrab()
    if restore_error is not None and not failed: raise restore_error
