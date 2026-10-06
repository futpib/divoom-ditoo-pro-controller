#!/usr/bin/env python3
"""Run hostile scripts and independent recovery queries in one USB or BLE session."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('device', nargs='?')
p.add_argument('--transport', choices=['ble','usb'], default='ble')
p.add_argument('--usb-port')
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--firmware', type=int, choices=[306013,306014,306015,306016,306017,306018,306019,306020,306021,306022,306023,306024,306025,306026,306027,306028,306029,306030,306031,306032,306033,306034,306035,306036], default=306036)
a = p.parse_args()
connection = ['--transport',a.transport]
if a.transport == 'ble':
    if not a.device: p.error('Bluetooth requires a device address')
    connection += ['--device',a.device]
if a.usb_port: connection += ['--usb-port',a.usb_port]
a.output.mkdir(parents=True, exist_ok=False)
# The CLI checks the installed firmware before using any extension selector.
preflight = subprocess.run([str(a.binary),*connection,
    'lua','stop'],capture_output=True,text=True,timeout=40,check=True)
assert json.loads(preflight.stdout)['abi'] == 2, 'Resident runtime required'
cases = [
    ('integer','return 6*7',2,'42'),
    ('float','return 7/2',2,'3.5'),
    ('libraries',"return string.upper('ditoo')..math.floor(math.sqrt(81))",2,'DITOO9'),
    ('loop','while true do end',3,None),
    ('pcall','while true do pcall(function() while true do end end) end',3,None),
    ('coroutine','pcall(function() coroutine.wrap(function() while true do end end)() end)',3,None),
    ('nested-coroutine','while true do coroutine.resume(coroutine.create(function() while true do end end)) end',3,None),
    ('finalizer','while true do local t=setmetatable({}, {__gc=function() while true do end end}) end',3,None),
    ('close','pcall(function() local x <close> = setmetatable({}, {__close=function() while true do end end}) end)',3,None),
    ('tostring',"tostring(setmetatable({}, {__tostring=function() while true do end end}))",3,None),
    ('sort','table.sort({3,2,1},function() while true do end end)',3,None),
    ('gsub',"('abc'):gsub('.',function() while true do end end)",3,None),
    ('pattern',"string.match(string.rep('a',100),string.rep('a*',20)..'b')",3,None),
    ('oom',"local t={}; while true do t[#t+1]=string.rep('x',100) end",3,None),
    ('caught-oom',"while true do pcall(string.rep,'a',2147483647) end",3,None),
    ('recursion','local function f() return 1+f() end; return f()',3,None),
    ('native-call-limit','while true do volume() end',3,None),
    ('syntax','return function (',3,None),
    ('binary','\x1bLua',3,None),
    ('capabilities','return io or os or package or debug or load or string.dump or string.format',2,'nil'),
]
if a.firmware >= 306014:
    cases += [
        ('stock-heap-headroom','return device.stats().free_heap >= 24576',2,'true'),
        ('led-frame',"lights.fill(0x123456); lights.pixel(0,0xffffff); return #lights.frame()..':'..lights.count",2,'36:12'),
        ('one-shot-present','display.clear(0); display.present(); return 42',2,'42'),
    ]
requests = []
resident_checks = []
def request(data, delay=0):
    requests.append({'command':'0x37','payload_hex':data.hex(),'response':'0x37','delay_ms':delay})
def op(code, data=b'', delay=0):
    request(b'\x7fDLUA'+bytes([code])+data,delay)
def status(name, state, result=None, reclaimed=False):
    resident_checks.append((len(requests),name,state,result,reclaimed))
    op(0)
def start(source, delay=250):
    source = source.encode()
    op(2,delay=100)
    op(3,len(source).to_bytes(2,'little')+b'\x01')
    for offset in range(0,len(source),512):
        op(4,offset.to_bytes(2,'little')+source[offset:offset+512])
    op(5,delay=delay)
start("local n=0; return {init=function() timer.every(40,function() n=n+1 end) end, "
      "update=function() display.clear(n); display.present() end, "
      "message=function(s) comms.send(s); app.log(s) end}")
status('resident-start',4)
op(8,b'ping',250)
status('incoming-message',4,'ping')
resident_checks.append((len(requests),'outgoing-message',4,'ping',False))
op(9)
resident_checks.append((len(requests),'outgoing-acknowledged',4,'',False))
op(9)
op(6,delay=100)
status('paused',5,'ping')
op(7,delay=250)
status('resumed',4,'ping')
op(2,delay=100)
status('stopped',2,'stopped',True)
for name,source in [
    ('update-loop','return {update=function() while true do end end}'),
    ('timer-loop','return {init=function() timer.after(10,function() while true do end end) end}'),
    ('message-loop','return {message=function() while true do end end}'),
]:
    start(source)
    if name == 'message-loop': op(8,b'trigger',250)
    status(name,3,reclaimed=True)
    start('return {}')
    status(name+'-recovery',4)
limit = 16384 if a.firmware >= 306030 else 8192
source='return {}\n--'+('x'*(limit-len('return {}\n--')))
start(source)
status(f'full-{limit}-byte-upload',4)
if a.firmware >= 306014:
    start('return {init=function() lights.fill(0x080008); lights.present() end, '
          'message=function() while true do end end}')
    status('led-resident',4)
    op(6,delay=100)
    status('led-paused',5)
    op(7,delay=250)
    status('led-resumed',4)
    op(8,b'crash',250)
    status('led-error-reclaimed',3,reclaimed=True)
    start('return {}')
    status('led-error-recovery',4)
op(2,delay=100)
status('final-stop',2,'stopped',True)
request(b'\0')
case_base = len(requests)
for name, source, state, value in cases:
    source = source.encode()
    request(b'\x7fDLUA\x01'+len(source).to_bytes(2,'little')+source,250)
    request(b'\x7fDLUA\0')
    request(b'\x7fDLUA\x01\x09\0return 42',250)
    request(b'\x7fDLUA\0')
request(b'\0')
(a.output/'requests.json').write_text(json.dumps(requests,indent=2)+'\n')
with (a.output/'replies.jsonl').open('w') as out, (a.output/'stderr.log').open('w') as err:
    run=subprocess.run([str(a.binary),*connection,'raw','run',str(a.output/'requests.json')],stdout=out,stderr=err,timeout=300)
rows=[json.loads(line) for line in (a.output/'replies.jsonl').read_text().splitlines()]
results=[]
for i,(name,source,state,value) in enumerate(cases):
    checks=[]
    for offset,expected,text in [(case_base+1+i*4,state,value),(case_base+3+i*4,2,'42')]:
        if offset>=len(rows):
            checks.append({'passed':False,'error':'missing response'});continue
        d=bytes.fromhex(rows[offset]['response']['data_hex'])
        message=d[40:].decode(errors='replace')
        ok=len(d)>=40 and d[:5]==b'DLUA\x02' and d[5]==expected and d[6]==0
        ok=ok and (text is None or message==text) and int.from_bytes(d[16:20],'little')==0
        checks.append({'passed':ok,'state':d[5] if len(d)>5 else None,'result':message,'peak_memory_bytes':int.from_bytes(d[8:12],'little'),'steps':int.from_bytes(d[12:16],'little')})
    results.append({'name':name,'source':source,'passed':all(c['passed'] for c in checks),'execution':checks[0],'recovery':checks[1]})
    print(name, 'passed' if results[-1]['passed'] else 'FAILED',flush=True)
for offset,name,state,value,reclaimed in resident_checks:
    d=bytes.fromhex(rows[offset]['response']['data_hex']) if offset<len(rows) else b''
    message=d[40:].decode(errors='replace')
    ok=len(d)>=40 and d[:5]==b'DLUA\x02' and d[5]==state and d[6]==0
    ok=ok and (value is None or message==value)
    ok=ok and (not reclaimed or int.from_bytes(d[16:20],'little')==0)
    results.append({'name':name,'passed':ok,'state':d[5] if len(d)>5 else None,
        'result':message,'memory_bytes':int.from_bytes(d[16:20],'little'),
        'frames':int.from_bytes(d[20:24],'little'),
        'callbacks':int.from_bytes(d[24:28],'little')})
    print(name,'passed' if ok else 'FAILED',flush=True)
healthy=len(rows)==len(requests) and rows[-1]['response']['data_hex']==(b'\x01'+a.firmware.to_bytes(4,'little')).hex()
report={'firmware':a.firmware,'checks':results,'stock_version_afterwards':healthy,'exit_code':run.returncode}
(a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
if run.returncode or not healthy or not all(r['passed'] for r in results): raise SystemExit(1)
