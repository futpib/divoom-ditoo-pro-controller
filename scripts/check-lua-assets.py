#!/usr/bin/env python3
"""Check stock image/glyph APIs over USB, then restore a supplied running app.

Volatile uploads only: no saved app, settings, pairing, or firmware writes.
Sound metadata is checked without playing audio. Raw transcripts stay private.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--restore-app', type=Path, required=True,
                        help='App source to start again in a finally block, including after failure')
    parser.add_argument('--dry-run', action='store_true', help='Prepare/validate the sequence without USB')
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    base = [str(args.binary.resolve()), '--transport', 'usb']

    def run(name, command, check=True):
        result = subprocess.run(base+command, capture_output=True, text=True, timeout=180)
        (args.output/(name+'.jsonl')).write_text(result.stdout)
        (args.output/(name+'.stderr')).write_text(result.stderr)
        if check:
            result.check_returncode()
        return result

    restore = args.output/'restore.lua'
    run('bundle-restore', ['lua', 'bundle', str(args.restore_app.resolve()), '-o', str(restore)])
    spec = importlib.util.spec_from_file_location('stock_assets', ROOT/'scripts/stock-assets.py')
    assets = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(assets)
    digest = 0x811c9dc5
    for frame in assets.frames(assets.stock_code()):
        for byte in frame['pixels']:
            digest = ((digest ^ byte)*0x01000193) & 0xffffffff
    signed_digest = digest if digest < 0x80000000 else digest-0x100000000
    expected = f'images 598 {signed_digest}'
    steps = [dict(op='start', file=str(ROOT/'tests/lua/assets-device.lua')),
             dict(op='wait', state='active', result=expected, timeout_ms=60000)]
    for cp in [65, 1046, 20013]:
        steps += [dict(op='send', message=str(cp)),
                  dict(op='wait', state='active', result_contains=f'glyph {cp} ')]
    steps += [dict(op='stop'), dict(op='wait', state='done', memory_bytes=0),
              dict(op='eval', source='return 42')]
    sequence = args.output/'sequence.json'
    sequence.write_text(json.dumps(steps, indent=2)+'\n')
    run('validate', ['lua', 'sequence', str(sequence), '--dry-run'])
    if args.dry_run:
        print(json.dumps(dict(dry_run=True, expected=expected, sequence=str(sequence))))
        return

    query = args.output/'version-request.json'
    query.write_text(json.dumps([dict(command='0x37', payload_hex='00', response='0x37')]))
    row = json.loads(run('version', ['raw', 'run', str(query)]).stdout.strip())
    if row['response']['data_hex'] != (b'\1'+(306026).to_bytes(4, 'little')).hex():
        raise ValueError('asset check requires 306026; no app changed')
    try:
        result = run('check', ['lua', 'sequence', str(sequence)], check=False)
        result.check_returncode()
        rows = [json.loads(line) for line in result.stdout.splitlines()]
        glyphs = {}
        for row in rows:
            if row['op'] != 'wait':
                continue
            value = row['status']['result']
            if value.startswith('glyph '):
                _, cp, data = value.split()
                decoded = bytes.fromhex(data)
                assert len(decoded) == 32 and decoded not in (bytes(32), b'\xff'*32)
                glyphs[cp] = data
        assert set(glyphs) == {'65', '1046', '20013'}
        assert rows[-1]['status']['state'] == 'done' and rows[-1]['status']['result'] == '42'
        summary = dict(firmware=306026, image_frames=598, image_fnv1a32=f'{digest:08x}',
                       image_read_matches_draw=True, glyphs=glyphs, stopped_memory_bytes=0,
                       recovery_result=42, sound_test='metadata only; no audio played')
    finally:
        restored = run('restore', ['lua', 'start', str(restore)])
        status = json.loads(restored.stdout)
        if status['state'] != 'active':
            raise RuntimeError('restoring app did not reach active state')
    summary['restore_active'] = True
    (args.output/'verification.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
