#!/usr/bin/env python3
"""Build the pinned probe, verify its boot, then restore and verify pinned stock."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def verified(path, version):
    events = [json.loads(line) for line in path.read_text().splitlines()]
    return (any(e.get('event') == 'device_update_complete' and e.get('version') == version
                for e in events)
            and bool(events) and events[-1].get('event') == 'verified'
            and events[-1].get('firmware_versions') == [version])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('device', nargs='?', help='Explicit Bluetooth MAC address')
    parser.add_argument('--dry-run', action='store_true', help='Build and validate both images without Bluetooth access')
    parser.add_argument('--output', type=Path, help='New directory for logs (must not exist)')
    parser.add_argument('--kernel-disconnect', action='store_true',
                        help='Use btmgmt between phases for the observed BlueZ disconnect stall; requires root or passwordless sudo')
    parser.add_argument('--inhibited', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.dry_run and not args.device:
        parser.error('device is required unless --dry-run is used')
    if args.output and args.output.exists():
        parser.error('--output directory must not already exist')
    if not args.dry_run and not args.inhibited:
        if not shutil.which('systemd-inhibit'):
            parser.error('systemd-inhibit is required for live flashing')
        return subprocess.call(['systemd-inhibit', '--what=sleep:idle', '--mode=block',
                                '--who=divoom-firmware-roundtrip', '--why=Ditoo firmware round trip',
                                sys.executable, str(Path(__file__).resolve()), *sys.argv[1:], '--inhibited'])

    disconnect = []
    if args.kernel_disconnect and not args.dry_run:
        btmgmt = shutil.which('btmgmt')
        if not btmgmt:
            parser.error('btmgmt is required by --kernel-disconnect')
        if os.geteuid() != 0:
            subprocess.run(['sudo', '-n', 'true'], check=True)
            disconnect = ['sudo', '-n']
        disconnect += [btmgmt, '--timeout', '10', 'disconnect', '-t', '1', args.device]

    subprocess.run([sys.executable, str(ROOT / 'scripts/build-reflash-probe.py')], check=True)
    target = ROOT / 'target/firmware-roundtrip'
    subprocess.run(['cargo', 'build', '--locked', '--release', '--no-default-features',
                    '--target-dir', str(target)], cwd=ROOT, check=True)
    binary = target / 'release/divoom-ditoo-pro-controller'
    for image in ['306008-reflash-probe.MVA', '306007.MVA']:
        subprocess.run([str(binary), 'firmware-update', str(ROOT / 'firmware' / image), '--dry-run'], check=True)
    if args.dry_run:
        return 0

    output = args.output or ROOT / 'firmware/runs' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    print(f'Logs: {output}', flush=True)
    env = {**os.environ, 'RUST_LOG': 'info'}
    with (output / 'preflight.jsonl').open('x') as stdout, (output / 'preflight.log').open('x') as stderr:
        preflight = subprocess.run([str(binary), '--device', args.device, '--transport', 'ble', 'firmware'],
                                   stdout=stdout, stderr=stderr, env=env)
    replies = [json.loads(line) for line in (output / 'preflight.jsonl').read_text().splitlines()]
    if not any((r.get('response') or {}).get('firmware_versions') == [306007] for r in replies):
        raise RuntimeError(f'Expected installed stock 306007 before starting; inspect {output}')
    if preflight.returncode:
        print('Preflight version was read, but connection cleanup failed; see preflight.log', file=sys.stderr)
    if disconnect:
        with (output / 'preflight-disconnect.log').open('x') as log:
            # Already disconnected is harmless before the first flash.
            subprocess.run(disconnect, stdout=log, stderr=subprocess.STDOUT, timeout=15)
    results = []
    for name, image, version, flags in [
        ('probe', '306008-reflash-probe.MVA', 306008, []),
        ('stock', '306007.MVA', 306007, ['--restore-stock']),
    ]:
        command = [str(binary), '--device', args.device, '--transport', 'ble',
                   'firmware-update', str(ROOT / 'firmware' / image), *flags]
        print(f'Starting {name}; progress is in {output / (name + ".jsonl")}', flush=True)
        with (output / (name + '.jsonl')).open('x') as stdout, (output / (name + '.log')).open('x') as stderr:
            result = subprocess.run(command, stdout=stdout, stderr=stderr, env=env)
        success = result.returncode == 0 and verified(output / (name + '.jsonl'), version)
        results.append({'phase': name, 'version': version, 'exit_code': result.returncode, 'verified': success})
        (output / 'result.json').write_text(json.dumps(results, indent=2) + '\n')
        if not success:
            raise RuntimeError(f'{name} was not verified; no further flash attempted. Inspect {output}')
        print(f'Verified {version}', flush=True)
        if disconnect:
            # The firmware version has already been verified; retain cleanup status separately.
            with (output / (name + '-disconnect.log')).open('x') as log:
                cleanup = subprocess.run(disconnect, stdout=log, stderr=subprocess.STDOUT, timeout=15)
            if cleanup.returncode != 0:
                print(f'Kernel cleanup returned {cleanup.returncode} after verified {version}; '
                      f'see {name}-disconnect.log (an already disconnected bearer is harmless)', file=sys.stderr)
    print('Verified 306007 -> modified 306008 -> stock 306007', flush=True)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, subprocess.SubprocessError, RuntimeError) as error:
        print(f'Round trip stopped: {error}', file=sys.stderr)
        sys.exit(1)
