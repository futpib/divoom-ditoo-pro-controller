#!/usr/bin/env python3
"""Inspect/navigate the on-device menu over USB (firmware 306028+).

status and screenshot read state; open exits Lua, and key operates the menu.
Screenshots contain the added menu's RGB framebuffer, not a camera image.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
commands = parser.add_subparsers(dest='command', required=True)
commands.add_parser('status')
commands.add_parser('open')
keys = {'back': 0, 'left': 2, 'right': 3, 'select': 4, 'close': 7}
commands.add_parser('key').add_argument('key', choices=keys)
commands.add_parser('screenshot').add_argument('output', type=Path)
args = parser.parse_args()
action = 1 if args.command == 'open' else 2 if args.command == 'key' else 0
key = keys[args.key] if args.command == 'key' else 0
requests = []
for chunk in range(6 if args.command == 'screenshot' else 1):
    payload = b'\x7fDLUA\x0e'+bytes([action, key, chunk])
    requests.append({'command': '0x37', 'payload_hex': payload.hex(), 'response': '0x37'})
with tempfile.TemporaryDirectory(prefix='ditoo-menu-') as directory:
    path = Path(directory)/'requests.json'
    path.write_text(json.dumps(requests))
    result = subprocess.run([str(args.binary), '--transport', 'usb', 'raw', 'run', str(path)],
                            check=True, capture_output=True, text=True, timeout=30)
rows = [bytes.fromhex(json.loads(line)['response']['data_hex']) for line in result.stdout.splitlines()]
if len(rows) != len(requests) or any(len(r) != 160 or r[:5] != b'DMNU\1' or r[5] for r in rows):
    raise SystemExit('Menu diagnostic rejected the request or firmware is older than 306028')
r = rows[0]
print(json.dumps(dict(page=r[6], index=r[7], native_id=r[8], count=r[9], open=bool(r[10]),
    custom_frame=bool(r[11]), operation=r[12], request=r[13], app_state=r[14], storage_ready=bool(r[15]),
    autostart=not r[17], bluetooth_mode=['app', 'remote', 'combined'][r[18]],
    keyboard_lights=['app', 'stock', 'off'][r[19]], indicator='off' if r[20] else 'auto',
    usb_mode='charge-control' if r[21] else 'audio-control', usb_pid=f'{int.from_bytes(r[24:26], "little"):04x}')))
if args.command == 'screenshot':
    if not r[11]:
        raise SystemExit('The added menu is not currently visible')
    args.output.write_bytes(b'P6\n16 16\n255\n'+b''.join(r[32:] for r in rows))
