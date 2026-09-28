#!/usr/bin/env python3
"""Read native Lua peripherals and optionally round-trip disabled saved settings.

Raw transport transcripts stay in the requested (normally ignored) output folder.
The settings test backs up all exposed fields before changing an unused slot.
It does not schedule a ringing alarm or record ambient audio.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


class Device:
    def __init__(self, device, output, binary):
        self.device, self.output, self.binary = device, output, binary
        self.requests, self.checks = [], []
        # The CLI rejects stock firmware before sending any extension selector.
        for attempt in range(3):
            preflight = subprocess.run([str(binary), '--device', device, '--transport', 'ble',
                                        'lua', 'stop'], capture_output=True, text=True,
                                       env={**os.environ, 'RUST_LOG': 'warn'}, timeout=45)
            # A completed script error is still a valid, reclaimed runtime.
            try:
                state = json.loads(preflight.stdout)
            except json.JSONDecodeError:
                state = {}
            if state.get('abi') == 2 and state.get('state') in ('idle', 'done', 'error') and state.get('memory_bytes') == 0:
                break
            if attempt == 2:
                raise RuntimeError('Lua preflight failed: '+preflight.stderr)
            time.sleep(1)
        assert state['abi'] == 2

    def op(self, code, data=b'', delay=0):
        self.requests.append({'command': '0x37', 'payload_hex': (b'\x7fDLUA'+bytes([code])+data).hex(),
                              'response': '0x37', 'delay_ms': delay})

    def case(self, name, source, delay=500):
        self.op(2, delay=150)
        encoded = source.encode()
        assert len(encoded) <= 8192
        self.op(3, len(encoded).to_bytes(2, 'little')+b'\x01')
        for offset in range(0, len(encoded), 512):
            self.op(4, offset.to_bytes(2, 'little')+encoded[offset:offset+512])
        self.op(5, delay=delay)
        self.checks.append((len(self.requests), name))
        self.op(0)

    def query(self, name, expression, fields):
        values = ','.join('tostring(v.'+f+')' for f in fields)
        self.case(name, 'local t; return {init=function() t=assert('+expression+') end, '
                  'update=function() local ok,v=device.result(t); if ok==nil then return end; '
                  'assert(ok,v); app.log(table.concat({'+values+'},",")) end}')

    def run(self, name):
        self.op(2, delay=150)
        request = self.output/(name+'-requests.json')
        request.write_text(json.dumps(self.requests, indent=2)+'\n')
        with (self.output/(name+'-replies.jsonl')).open('w') as out, (self.output/(name+'.log')).open('w') as err:
            run = subprocess.run([str(self.binary), '--device', self.device, '--transport', 'ble',
                                  'raw', 'run', str(request)], stdout=out, stderr=err,
                                 env={**os.environ, 'RUST_LOG': 'warn'}, timeout=240)
        rows = [json.loads(line) for line in (self.output/(name+'-replies.jsonl')).read_text().splitlines()]
        results = {}
        for index, label in self.checks:
            assert index < len(rows), (label, 'missing reply')
            data = bytes.fromhex(rows[index]['response']['data_hex'])
            result = data[40:].decode()
            assert data[:7] == b'DLUA\x02\x04\x00', (label, data.hex(), result)
            assert result, (label, 'operation not complete')
            results[label] = result
            print(label, result, flush=True)
        assert run.returncode == 0 and len(rows) == len(self.requests)
        self.requests, self.checks = [], []
        return results


ALARM_FIELDS = ['enabled', 'hour', 'min', 'days', 'mode', 'trigger', 'volume']
WAKE_FIELDS = ['enabled', 'action', 'hour', 'min', 'days', 'color']


def config(fields, result):
    data = dict(zip(fields, result.split(','), strict=True))
    return '{'+','.join(key+'='+('"'+value+'"' if key == 'action' else value)
                        for key, value in data.items())+'}'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('device')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
    p.add_argument('--settings-test', action='store_true')
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    d = Device(args.device, args.output, args.binary)
    for slot in range(10):
        d.query('alarm-'+str(slot), 'alarm.get('+str(slot)+')', ALARM_FIELDS)
    for slot in range(9):
        d.query('wake-'+str(slot), 'power.get_schedule('+str(slot)+')', WAKE_FIELDS)
    d.case('battery', 'return {update=function() local b=power.battery(); '
           'app.log(table.concat({b.level,b.max_level,tostring(b.external_power),tostring(b.charging),tostring(b.full)},",")) end}')
    d.case('audio', 'return {update=function() local a=audio.status(); '
           'app.log(table.concat({tostring(a.playing),a.volume,a.track,a.tracks,tostring(a.recording)},",")) end}')
    baseline = d.run('baseline')
    (args.output/'baseline.json').write_text(json.dumps(baseline, indent=2)+'\n')
    report = {'baseline': baseline}
    if args.settings_test:
        alarm_slot = next(i for i in reversed(range(10)) if baseline['alarm-'+str(i)].startswith('false,'))
        wake_slot = next(i for i in reversed(range(9)) if baseline['wake-'+str(i)].startswith('false,'))
        original_alarm = config(ALARM_FIELDS, baseline['alarm-'+str(alarm_slot)])
        original_wake = config(WAKE_FIELDS, baseline['wake-'+str(wake_slot)])
        try:
            d.query('alarm-save', f'alarm.set({alarm_slot},{{enabled=false,hour=11,min=23,days=62,mode=2,trigger=1,volume=15}})', ALARM_FIELDS)
            d.query('alarm-read', f'alarm.get({alarm_slot})', ALARM_FIELDS)
            d.query('wake-save', f'power.set_schedule({wake_slot},{{enabled=false,action="on",hour=7,min=21,days=127,color=0x123456}})', WAKE_FIELDS)
            d.query('wake-read', f'power.get_schedule({wake_slot})', WAKE_FIELDS)
            changed = d.run('settings')
            assert changed['alarm-save'] == changed['alarm-read'] == 'false,11,23,62,2,1,15'
            assert changed['wake-save'] == changed['wake-read'] == 'false,on,7,21,127,1193046'
            report['settings'] = changed
        finally:
            d.requests, d.checks = [], []
            d.query('alarm-restored', f'alarm.set({alarm_slot},{original_alarm})', ALARM_FIELDS)
            d.query('alarm-restored-read', f'alarm.get({alarm_slot})', ALARM_FIELDS)
            d.query('wake-restored', f'power.set_schedule({wake_slot},{original_wake})', WAKE_FIELDS)
            restored = d.run('restore')
            assert restored['alarm-restored'] == baseline['alarm-'+str(alarm_slot)]
            assert restored['wake-restored'] == baseline['wake-'+str(wake_slot)]
            report['restored'] = restored
    (args.output/'results.json').write_text(json.dumps(report, indent=2)+'\n')


if __name__ == '__main__':
    main()
