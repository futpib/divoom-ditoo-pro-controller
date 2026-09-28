#!/usr/bin/env python3
"""Check or synchronize Lua firmware registrations from a validated build report.

Always prints the proposed diff. Only --update writes source files; no build,
flash, Git operation, or device connection is performed.
"""
import argparse
import binascii
import difflib
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def replace_once(text, pattern, replacement):
    result, count = re.subn(pattern, replacement, text)
    if count != 1:
        raise ValueError(f'expected one source anchor, found {count}: {pattern}')
    return result


def load_image(root, image):
    report = json.loads(image.with_suffix('.json').read_text())
    data = image.read_bytes()
    version = report['version']
    if not isinstance(version, int) or not 306012 <= version < 307000:
        raise ValueError('tool supports the 306012+ Lua runtime build reports')
    if image.name != f'{version}-lua.MVA':
        raise ValueError('image filename and report version disagree')
    actual = dict(version=version, bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), checksum=sum(data))
    for key, value in actual.items():
        if report.get(key) != value:
            raise ValueError(f'build report {key} mismatch: expected {value}, got {report.get(key)}')
    spec = importlib.util.spec_from_file_location('reflash_base', root/'scripts/build-reflash-probe.py')
    base = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(base)
    stock = (root/'firmware/306007.MVA').read_bytes()
    if hashlib.sha256(stock).hexdigest() != base.STOCK_SHA:
        raise ValueError('stock reference hash mismatch')
    if len(data) < base.CODE + 0x47928 + 4 or len(data) > base.CODE + 0x1f0000 + 4:
        raise ValueError('code image outside application bounds')
    if data[:0x607] != stock[:0x607] or data[0x60b:base.CODE] != stock[0x60b:base.CODE]:
        raise ValueError('MVA header, flash driver, or load address differs from pinned stock')
    if struct.unpack_from('<I', data, 0x607)[0] != len(data) - 0x60b - 4:
        raise ValueError('MVA code record length mismatch')
    if struct.unpack_from('<I', data, len(data)-4)[0] != binascii.crc_hqx(data[:-4], 0):
        raise ValueError('MVA package CRC mismatch')
    code = data[base.CODE:-4]
    stock_code = stock[base.CODE:-4]
    if code[:0xcc] != stock_code[:0xcc] or code[0xd4:0x10000] != stock_code[0xd4:0x10000]:
        raise ValueError('bootloader differs from pinned stock outside length/CRC fields')
    if struct.unpack_from('<I', code, 0xd0)[0] & 0xffffff != len(code):
        raise ValueError('code length header mismatch')
    if int.from_bytes(code[0x47924:0x47928], 'big') != 0x44000000 | version:
        raise ValueError('native version instruction disagrees with build report')
    if base.crc_code(code, 0x9e80) != 0x5f08 or base.crc_code(code, len(code)) != struct.unpack_from('<I', code, 0xcc)[0]:
        raise ValueError('internal boot/code CRC mismatch')
    actual['application_bytes'] = len(code)-0x10000
    actual['blocks'] = (actual['application_bytes']+4095)//4096
    return actual


def plan(root, image, meta, register=None, builder=None):
    before, after = {}, {}

    def read(name):
        before[name] = (root/name).read_text()
        after[name] = before[name]
        return before[name]

    version, sha, size = meta['version'], meta['sha256'], meta['bytes']
    name = 'src/firmware.rs'
    firmware = read(name)
    versions = dict((m[1], int(m[2])) for m in re.finditer(r'const (\w*VERSION): u32 = (\d+);', firmware))
    candidates = [key.removesuffix('VERSION') for key, value in versions.items() if value == version]
    if len(candidates) > 1:
        raise ValueError('ambiguous firmware version constants')
    prefix = candidates[0] if candidates else None
    if register:
        if not re.fullmatch(r'[A-Z][A-Z0-9_]*', register):
            raise ValueError('--register must be an uppercase identifier, e.g. KEYBOARD')
        if prefix and prefix != register+'_':
            raise ValueError('version is already registered under a different name')
        if not prefix and register+'_VERSION' in versions:
            raise ValueError('registration name is already used by another version')
    if prefix is None:
        if not register:
            raise ValueError('unregistered version; use --register NAME to propose a new registration')
        if version <= max(versions.values()):
            raise ValueError('new registration must be newer than all existing versions')
        prefix = register+'_'
        firmware = replace_once(firmware, r'const CHUNK_SIZE:',
            f'const {prefix}VERSION: u32 = {version};\nconst {prefix}SHA256: &str = "{sha}";\nconst {prefix}SIZE: usize = {size};\nconst CHUNK_SIZE:')
        firmware = replace_once(firmware, r'      _ => return Err\("Unrecognized firmware image:',
            f'      {prefix}SHA256 => {prefix}VERSION,\n      _ => return Err("Unrecognized firmware image:')
        firmware = replace_once(firmware, r'      _ => IMAGE_SIZE,', f'      {prefix}VERSION => {prefix}SIZE,\n      _ => IMAGE_SIZE,')
        # Update the supported restore boundary without accepting arbitrary future versions.
        highest = max(versions.values())
        marker = f'assert!(validate_target({highest+1}, VERSION, false, true).is_err());'
        if marker not in firmware:
            raise ValueError('missing restore boundary test')
        firmware = firmware.replace(marker, f'assert!(validate_target({version}, VERSION, false, true).is_ok());\n    assert!(validate_target({version+1}, VERSION, false, true).is_err());')
        # The restore allowlist lives in validate_target, before the tests.
        end = firmware.index('#[cfg(test)]')
        production, tests = firmware[:end], firmware[end:]
        production = replace_once(production, r'(if restore_stock\s*&& !\(matches!\([\s\S]*?)(\n\s*\))',
            lambda m: m[1] + f' | {prefix}VERSION' + m[2])
        firmware = production+tests
        firmware = firmware.replace(f'/{highest} are supported', f'/{highest}/{version} are supported')
        firmware = firmware.replace(f'through {highest})', f'through {version})')
    firmware = replace_once(firmware, rf'const {prefix}SHA256: &str = "[a-f0-9]+";', f'const {prefix}SHA256: &str = "{sha}";')
    firmware = replace_once(firmware, rf'(const {prefix}SIZE: usize = )([\d_]+);',
        lambda m:m[0] if int(m[2].replace('_','')) == size else f'{m[1]}{size:_};')
    for pattern in [rf'{prefix}SHA256\s*=>\s*{prefix}VERSION', rf'{prefix}VERSION\s*=>\s*{prefix}SIZE']:
        if not re.search(pattern, firmware):
            raise ValueError(f'incomplete firmware registration: {pattern}')
    # Older images have checksum/metadata assertions rather than only hash pins.
    test_pattern = r'(fn \w+\([^\n]*[\s\S]*?\n  \})'
    def sync_test(match):
        block = match[0]
        if f'/firmware/{image.name}' not in block:
            return block
        block = re.sub(r'assert_eq!\(image.checksum, ([\d_]+)\);',
            lambda m:m[0] if int(m[1].replace('_','')) == meta['checksum'] else f'assert_eq!(image.checksum, {meta["checksum"]});', block)
        return block
    at = firmware.index('#[cfg(test)]')
    firmware = firmware[:at]+re.sub(test_pattern, sync_test, firmware[at:])
    after[name] = firmware
    lua = read('src/lua.rs')
    constant = 'VERSION' if prefix == 'LUA_' else f'{prefix}VERSION'
    existing = re.search(rf'pub const {constant}: u32 = (\d+);', lua)
    if existing and int(existing[1]) != version:
        raise ValueError('Lua version constant disagrees with firmware registration')
    if not existing:
        lua = replace_once(lua, r'pub const APP_SOURCE_LIMIT:', f'pub const {constant}: u32 = {version};\npub const APP_SOURCE_LIMIT:')
        lua = replace_once(lua, r'(if !matches!\(\s*installed,[\s\S]*?)(\n\s*\))', lambda m:m[1]+f' | {constant}'+m[2])
        lua = lua.replace('; no program sent', f', {{{constant}}}; no program sent')
    if constant not in lua[lua.index('if !matches!'):lua.index('if installed == VERSION')]:
        raise ValueError('Lua firmware allowlist is missing registered version')
    after['src/lua.rs'] = lua
    usb = read('src/usb_firmware.rs')
    row = f'      ("{image.name}", {meta["application_bytes"]:_}, {meta["blocks"]}),'
    pattern = rf'      \("{re.escape(image.name)}", ([\d_]+), ([\d_]+)\),'
    if re.search(pattern, usb):
        usb = replace_once(usb, pattern, lambda m:m[0]
            if (int(m[1].replace('_','')), int(m[2].replace('_',''))) == (meta['application_bytes'], meta['blocks']) else row)
    else:
        usb = replace_once(usb, r'(      \("306\d+-lua.MVA", [\d_]+, [\d_]+\),\n)(    \] \{)', lambda m:m[1]+row+'\n'+m[2])
    after['src/usb_firmware.rs'] = usb
    checks = read('scripts/check-lua-app-device.py')
    match = re.search(r"choices=\[([\d, ]+)\], default=(\d+)", checks)
    if not match:
        raise ValueError('missing device-check firmware choices')
    choices = sorted(set([int(s) for s in match[1].split(',')]+([version] if version >= 306013 else [])))
    checks = checks[:match.start()]+f'choices={str(choices).replace(" ", "")}, default={max(choices)}'+checks[match.end():]
    after['scripts/check-lua-app-device.py'] = checks
    if version >= 306015:
        backup = read('scripts/backup-lua-storage.py')
        match = re.search(r'assert installed in \(([\d, ]+)\)', backup)
        if not match:
            raise ValueError('missing storage diagnostic firmware allowlist')
        allowed = sorted(set([int(s) for s in match[1].split(',')]+[version]))
        backup = backup[:match.start(1)]+','.join(map(str, allowed))+backup[match.end(1):]
        if version not in [int(s) for s in match[1].split(',')]:
            backup = re.sub(r'306015-306\d+ diagnostic', f'306015-{max(allowed)} diagnostic', backup)
            backup = re.sub(r"'Firmware 306015[^'\n]+required; no diagnostic sent'",
                            "'Unsupported Lua storage diagnostic firmware; no diagnostic sent'", backup)
        after['scripts/backup-lua-storage.py'] = backup
    if builder:
        source = (root/builder).read_text()
        versions_in_builder = set()
        for match in re.finditer(r"'version':\s*(306\d+)|firmware/(306\d+)-lua", source):
            versions_in_builder.add(int(match[1] or match[2]))
        if versions_in_builder != {version}:
            raise ValueError(f'builder output/report versions disagree: {sorted(versions_in_builder)}')
        opcode = f'{0x44000000 | version:08x}'
        if f"bytes.fromhex('{opcode}')" not in source:
            raise ValueError('builder version instruction does not match image')
    return before, after


def apply(root, before, after):
    changes = {name:text for name,text in after.items() if text != before[name]}
    # Detect concurrent edits before replacing anything. Stage all output first.
    for name in before:
        if (root/name).read_text() != before[name]:
            raise ValueError(f'file changed during planning: {name}; rerun')
    staged = []
    try:
        for name, text in changes.items():
            path = root/name
            with tempfile.NamedTemporaryFile(mode='w', dir=path.parent, delete=False) as out:
                out.write(text)
                temporary = Path(out.name)
            staged.append((temporary, path))
            temporary.chmod(path.stat().st_mode)
        for temporary, path in staged:
            os.replace(temporary, path)
    finally:
        for temporary, _ in staged:
            temporary.unlink(missing_ok=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('image', type=Path)
    p.add_argument('--root', type=Path, default=ROOT)
    mode = p.add_mutually_exclusive_group()
    mode.add_argument('--check', action='store_true', help='Default: report drift without writing')
    mode.add_argument('--update', action='store_true', help='Apply the printed source diff')
    p.add_argument('--register', help='Name for a new version constant; never inferred')
    p.add_argument('--builder', type=Path, help='Also check this builder outputs the selected version')
    a = p.parse_args()
    try:
        image = a.image.resolve()
        meta = load_image(a.root, image)
        before, after = plan(a.root, image, meta, a.register, a.builder)
        changed = []
        for name in before:
            if before[name] != after[name]:
                changed.append(name)
                sys.stdout.writelines(difflib.unified_diff(before[name].splitlines(True), after[name].splitlines(True), fromfile=name, tofile=name))
        if a.update:
            apply(a.root, before, after)
        print(json.dumps(dict(meta, changed_files=changed, updated=bool(a.update and changed))))
        return int(bool(changed) and not a.update)
    except (OSError, ValueError, KeyError) as error:
        print(f'firmware-manifest: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
