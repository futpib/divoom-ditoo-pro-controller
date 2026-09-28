#!/usr/bin/env python3
"""Exercise USB control without Bluetooth; temporarily changes display/settings.

Stops the current Lua app. Restores volume, brightness and 12/24-hour setting,
then selects the original clock face. Does not modify alarms or power schedules.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
p.add_argument('--usb-port')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
base = [str(a.binary),'--transport','usb']
if a.usb_port: base += ['--usb-port',a.usb_port]
checks = []
def command(*args, timeout=30):
    started = time.monotonic()
    result = subprocess.run(base+list(args), text=True, capture_output=True,
                            env={**os.environ,'RUST_LOG':'warn'},timeout=timeout)
    with (a.output/'commands.jsonl').open('a') as out:
        out.write(json.dumps({'args':args,'stdout':result.stdout,'stderr':result.stderr,
            'exit_code':result.returncode,'seconds':time.monotonic()-started})+'\n')
    result.check_returncode()
    return json.loads(result.stdout) if result.stdout.strip() else None

def check(name, condition):
    checks.append({'name':name,'passed':bool(condition)})
    assert condition, name

command('lua','stop')
status = command('status')['response']
volume = command('volume','get')
clock = command('clock','get')
hour24 = int(command('setting','hour24')['response']['data_hex'],16)
try:
    version = command('firmware')['response']
    check('running-version', version['data_hex']=='0163ab0400')
    for args in [('alarms',),('playback-status',),('sd-status',),
                 ('setting','save-volume'),('setting','auto-connect'),
                 ('setting','idle-power-off'),('setting','startup-channel')]:
        reply = command(*args)
        check('read-'+':'.join(args), reply['transport']=='usb' and reply['response']['ack'])
    command('volume','set','0')
    check('volume-roundtrip',command('volume','get')==0)
    command('brightness','12')
    check('brightness-roundtrip',command('status')['response']['brightness']==12)
    command('setting','hour24',str(1-hour24))
    check('setting-roundtrip',int(command('setting','hour24')['response']['data_hex'],16)==1-hour24)
    command('scoreboard','12','34')
    score = command('tool-status','1')['response']['data_hex']
    check('scoreboard-query',len(bytes.fromhex(score))==6)
    # Retain values: this stock tool may acknowledge a setter without applying it.
    checks[-1]['data_hex'] = score
    checks[-1]['scores_applied'] = '0c002200' in score
    command('game','6');command('game-key','1');command('game-key','1','--release');command('game','6','--exit')
    check('game-and-keys-dispatched',True)
    command('image',str(ROOT/'images/bunny.gif'))
    command('animation',str(ROOT/'images/bunny.divoom16'),timeout=60)
    check('image-and-animation-dispatched',True)
    result = command('lua','eval','return string.rep("x",191)')
    check('fragmented-response',result['result']=='x'*191 and result['state']=='done')
    payload = b'\x7fDLUA\x00'+bytes(4093-6)
    reply = command('raw','send','0x37','--data',payload.hex(),'--query')
    check('maximum-4096-byte-native-command',reply['response']['data_hex'].startswith('444c554102'))
    result = command('lua','eval','return tostring(bluetooth.status().ble_connected)')
    check('device-confirms-no-ble-connection',result['result']=='false')
    command('clock','set',str(clock))
    delayed = a.output/'delayed.json'
    delayed.write_text(json.dumps([
        {'command':'0x37','payload_hex':'00','response':'0x37','delay_ms':16000},
        {'command':'0x37','payload_hex':'00','response':'0x37'}]))
    run = subprocess.run(base+['raw','run',str(delayed)],text=True,capture_output=True,
                         env={**os.environ,'RUST_LOG':'warn'},timeout=30,check=True)
    rows = [json.loads(line) for line in run.stdout.splitlines()]
    check('session-survives-long-script-delay',len(rows)==2 and
          all(row['response']['data_hex']=='0163ab0400' for row in rows))
    events = subprocess.run(base+['raw','monitor','--seconds','15'],capture_output=True,text=True,
                            env={**os.environ,'RUST_LOG':'warn'},timeout=25,check=True)
    (a.output/'events.jsonl').write_text(events.stdout)
    event_rows = [json.loads(line) for line in events.stdout.splitlines()]
    check('asynchronous-native-notification',any(row.get('opcode')==0xf7 or
          row.get('response',{}).get('opcode')==0xf7 for row in event_rows))
finally:
    command('volume','set',str(volume))
    command('brightness',str(status['brightness']))
    command('setting','hour24',str(hour24))
    command('clock','set',str(clock))
report = {'firmware':306019,'transport':'usb','checks':checks,
          'all_passed':all(c['passed'] for c in checks),
          'display_delivery':'native dispatch acknowledged; no optical screen comparison'}
(a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
