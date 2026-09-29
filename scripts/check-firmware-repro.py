#!/usr/bin/env python3
"""Rebuild every retained builder twice in isolated paths; compare committed bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from firmware_patches import load_manifest

ROOT = Path(__file__).resolve().parents[1]
BUILDS = [('reflash','build-reflash-probe.py','reflash-probe','target/reflash'),
          ('probe','build-lua-probe.py','lua-probe','target/lua'),
          ('runtime','build-lua-runtime.py','lua','target/lua/runtime'),
          ('app','build-lua-app-runtime.py','lua','target/lua-app/runtime')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'target/firmware-review')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = load_manifest(ROOT)
    results = []
    with tempfile.TemporaryDirectory(prefix='ditoo-firmware-repro-') as tmp:
        for run in ['first', 'different-path-second']:
            root = Path(tmp)/run
            root.mkdir()
            for directory in ['native','scripts','firmware']:
                shutil.copytree(ROOT/directory,root/directory,
                                ignore=shutil.ignore_patterns('__pycache__','runs','decoded'))
            for profile, script, suffix, out in BUILDS:
                version = manifest['profiles'][profile]['version']
                stem = f'{version}-{suffix}'
                expected = {ext:(ROOT/f'firmware/{stem}.{ext}').read_bytes() for ext in ['MVA','json']}
                log = args.output/f'{run}-{profile}.log'
                with log.open('w') as stream:
                    subprocess.run(['python3','scripts/'+script],cwd=root,stdout=stream,
                                   stderr=subprocess.STDOUT,check=True)
                for ext, data in expected.items():
                    built = (root/f'firmware/{stem}.{ext}').read_bytes()
                    if built != data:
                        raise ValueError(f'{run}: {stem}.{ext} differs from checked-in artifact; see {log}')
                review = root/out
                for ext in ['json','md']:
                    dest = args.output/f'{profile}-patches.{ext}'
                    data = (review/f'patch-report.{ext}').read_bytes()
                    if run == 'first':
                        dest.write_bytes(data)
                    elif dest.read_bytes() != data:
                        raise ValueError(f'{profile}: review report differs between builds')
                row = dict(run=run,profile=profile,version=version,
                           sha256=hashlib.sha256(expected['MVA']).hexdigest(),identical=True)
                results.append(row)
                print(json.dumps(row),flush=True)
    (args.output/'verification.json').write_text(json.dumps(results,indent=2)+'\n')


if __name__ == '__main__':
    main()
