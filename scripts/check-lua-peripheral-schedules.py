#!/usr/bin/env python3
"""Exercise native alarm/snooze and an off/wake cycle using unused slots.

Requires disabled alarm 9 and power schedules 7/8. Saves their exposed settings,
restores them afterward, and leaves raw BLE logs in the specified output folder.
The test sounds an alarm briefly and powers the device off for one minute.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('peripherals', ROOT/'scripts/check-lua-peripherals.py')
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('device')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--usb-port', required=True)
p.add_argument('--exercise-schedules', action='store_true', required=True)
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
d = checks.Device(a.device, a.output, a.binary)
d.query('alarm-9', 'alarm.get(9)', checks.ALARM_FIELDS)
for slot in (7, 8):
    d.query('wake-'+str(slot), f'power.get_schedule({slot})', checks.WAKE_FIELDS)
baseline = d.run('baseline')
(a.output/'baseline.json').write_text(json.dumps(baseline, indent=2)+'\n')
assert all(value.startswith('false,') for value in baseline.values()), 'Test slots must be disabled'
report = {'baseline': baseline}


def status():
    try:
        r = subprocess.run([str(a.binary), '--device', a.device, '--transport', 'ble',
                            'lua', 'status'], capture_output=True, text=True, timeout=12,
                           env={**os.environ, 'RUST_LOG': 'warn'})
        return {'exit_code': r.returncode, 'stdout': r.stdout, 'stderr': r.stderr}
    except subprocess.TimeoutExpired:
        return {'timed_out': True}


def wait_until(deadline):
    while time.monotonic() < deadline:
        time.sleep(max(0, min(1, deadline-time.monotonic())))


try:
    d.case('alarm-start', (ROOT/'firmware/lua-peripherals-evidence/alarm-fire.lua').read_text(), 45000)
    d.checks.clear()
    d.op(0, delay=45000)
    d.checks.append((len(d.requests), 'alarm'))
    d.op(0)
    report['alarm'] = d.run('alarm')
    assert report['alarm']['alarm'] == 'ring-snooze-cancel-restore passed'
    d.case('wake', (ROOT/'firmware/lua-peripherals-evidence/wake-cycle.lua').read_text(), 2000)
    report['armed'] = d.run('armed')
    delay = int(report['armed']['wake'].split(':')[1])
    assert 0 < delay < 120
    off = time.monotonic()+delay
    usb = Path('/sys/bus/usb/devices')/a.usb_port
    wait_until(off+8)
    report['usb_absent_while_off'] = not usb.exists()
    report['off_status'] = status()
    wait_until(off+70)
    report['usb_present_after_wake'] = usb.exists()
    report['on_status'] = status()
    assert report['usb_absent_while_off'] and report['usb_present_after_wake']
    assert report['off_status'].get('exit_code') != 0
    assert report['on_status'].get('exit_code') == 0
    boot = json.loads(report['on_status']['stdout'])
    assert boot['state'] == 'idle' and boot['generation'] == 0
finally:
    try:
        d = checks.Device(a.device, a.output, a.binary)
        old = checks.config(checks.ALARM_FIELDS, baseline['alarm-9'])
        d.query('alarm-9', f'alarm.set(9,{old})', checks.ALARM_FIELDS)
        for slot in (7, 8):
            old = checks.config(checks.WAKE_FIELDS, baseline['wake-'+str(slot)])
            d.query('wake-'+str(slot), f'power.set_schedule({slot},{old})', checks.WAKE_FIELDS)
        report['restored'] = d.run('restored')
        assert report['restored'] == baseline
    finally:
        (a.output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
