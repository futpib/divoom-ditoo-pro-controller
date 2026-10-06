"""Apply named, assembled patches to pinned stock firmware and emit review reports."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tomllib

MANIFEST = 'native/patches/manifest.toml'


def load_manifest(root):
    data = tomllib.loads((Path(root)/MANIFEST).read_text())
    if data.get('format') != 2:
        raise ValueError('unsupported patch manifest format')
    patches = data['patches']
    names = [p['name'] for p in patches]
    if len(names) != len(set(names)):
        raise ValueError('duplicate patch name')
    for p in patches:
        if not re.fullmatch(r'[a-z][a-z0-9_]*', p['name']) or not p['purpose'].strip():
            raise ValueError('patch must have a name and purpose')
        if not re.fullmatch(r'\.[a-z][a-z0-9_]*', p['section']) or not re.fullmatch(r'\w+', p['site']):
            raise ValueError('invalid patch section or site')
        if not 0 < p['max_size'] <= p['original_size'] <= 256:
            raise ValueError(f'{p["name"]}: original instructions must cover the overwrite budget')
    for entry in [*patches, *data.get('guards', [])]:
        if not re.fullmatch(r'[a-zA-Z_]\w*', entry['site']):
            raise ValueError('invalid original instruction site')
        if not entry['original'].strip() or re.search(r'^\s*\.', entry['original'], re.M):
            raise ValueError('original must contain instructions, not assembler data directives')
        if not 0 < entry['original_size'] <= 256 or not entry['purpose'].strip():
            raise ValueError('original instructions need a bounded size and purpose')
    guards = data.get('guards', [])
    if len({g['site'] for g in guards}) != len(guards):
        raise ValueError('duplicate ABI guard')
    for guard in guards:
        if not set(guard['profiles']) <= set(data['profiles']):
            raise ValueError('unknown ABI guard profile')
    for profile in data['profiles'].values():
        for name in profile.get('source_patches', []):
            if Path(name).is_absolute() or '..' in Path(name).parts or not name.endswith('.patch'):
                raise ValueError('invalid source patch path')
        selected = profile['patches']
        if len(selected) != len(set(selected)) or not set(selected) <= set(names):
            raise ValueError('unknown or repeated patch in profile')
        sections = [p['section'] for p in patches if p['name'] in selected]
        if len(sections) != len(set(sections)):
            raise ValueError('duplicate patch section in profile')
    return data


def apply_patches(code, patches, symbols, sections, originals):
    """Validate the entire edit set before returning a modified copy."""
    planned = []
    for p in patches:
        address = symbols[p['site']]
        replacement = sections[p['section']]
        limit = p['max_size']
        if not replacement or len(replacement) > limit:
            raise ValueError(f'{p["name"]}: empty replacement or overwrite budget exceeded')
        expected = originals[p['site']]
        if len(expected) != p['original_size'] or len(expected) < limit:
            raise ValueError(f'{p["name"]}: original assembly size mismatch')
        if address < 0 or address + len(expected) > len(code):
            raise ValueError(f'{p["name"]}: site outside stock code')
        if code[address:address+len(expected)] != expected:
            raise ValueError(f'{p["name"]}: original bytes differ at {address:#x}')
        planned.append((address, address+limit, p, replacement))
    planned.sort(key=lambda row: row[0])
    for previous, current in zip(planned, planned[1:]):
        if previous[1] > current[0]:
            raise ValueError(f'overlapping patches: {previous[2]["name"]} and {current[2]["name"]}')
    result = bytearray(code)
    for start, _, _, replacement in planned:
        result[start:start+len(replacement)] = replacement
    return result


class PatchSet:
    def __init__(self, root, profile, out):
        self.root, self.out, self.profile = Path(root), Path(out), profile
        self.manifest = load_manifest(root)
        selected = self.manifest['profiles'][profile]
        self.version = selected['version']
        by_name = {p['name']: p for p in self.manifest['patches']}
        self.patches = [by_name[n] for n in selected['patches']]
        self.out.mkdir(parents=True, exist_ok=True)
        self.symbols, self.sections, self.rows = {}, {}, []
        self.guards = [g for g in self.manifest.get('guards', []) if profile in g['profiles']]
        self.originals, self.guard_rows = {}, []
        self.source_patches = selected.get('source_patches', [])

    def run(self, *args):
        return subprocess.check_output(args, cwd=self.root, text=True)

    def prepare_sources(self, source):
        """Apply the profile's ordered unified diffs to a fresh vendored source tree."""
        for name in self.source_patches:
            path = self.root/'native/patches'/name
            subprocess.run(['patch', '--batch', '--forward', '--fuzz=0',
                            '--no-backup-if-mismatch', '-p1', '-i', str(path)],
                           cwd=source, check=True)

    def prepare_originals(self):
        """Assemble stock instructions separately; these sections are never flashed."""
        source = ['.flag verbatim']
        layout = ['INCLUDE native/patches/stock-306007.ld', 'SECTIONS {']
        checks = [*self.patches, *self.guards]
        for i, entry in enumerate(checks):
            section = f'.original_{i}'
            source += [f'.section {section},"ax"', entry['original']]
            layout += [f'{section} {entry["site"]} : {{ KEEP(*({section})) }}',
                       f'ASSERT(SIZEOF({section}) == {entry["original_size"]}, "{entry["site"]}: original size")']
        layout += [' /DISCARD/ : { *(.comment) *(.note*) }', '}']
        (self.out/'originals.S').write_text('\n'.join(source)+'\n')
        (self.out/'originals.ld').write_text('\n'.join(layout)+'\n')
        self.run('nds32le-elf-gcc','-mcpu=d1088-spu','-mabi=2','-c',
                 str(self.out/'originals.S'),'-o',str(self.out/'originals.o'))
        elf = self.out/'originals.elf'
        self.run('nds32le-elf-ld','--no-relax','-T',str(self.out/'originals.ld'),
                 '-o',str(elf),str(self.out/'originals.o'))
        for i, entry in enumerate(checks):
            self.originals[entry['site']] = self.extract(f'.original_{i}',elf)

    def prepare(self):
        """Generate checked section placement, then assemble the readable stock edits."""
        layout = ['/* Generated from native/patches/manifest.toml; do not edit. */']
        for p in self.patches:
            section = p['section']
            layout += [f'{section} {p["site"]} : {{ KEEP(*({section})) }}',
                       f'ASSERT(SIZEOF({section}) > 0 && SIZEOF({section}) <= {p["max_size"]}, "{p["name"]}: overwrite budget")']
        (self.out/'patch-layout.ld').write_text('\n'.join(layout)+'\n')
        self.prepare_originals()
        obj = self.out/'stock-edits.o'
        self.run('nds32le-elf-gcc', '-mcpu=d1088-spu', '-mabi=2', '-mno-fp-as-gp',
                 f'-DFIRMWARE_VERSION={self.version}', f'-DPROFILE_{self.profile.upper()}',
                 '-c', 'native/patches/stock-edits.S', '-o', str(obj))
        return str(obj)

    def apply(self, stock, elf):
        if hashlib.sha256(stock).hexdigest() != self.manifest['stock_sha256']:
            raise ValueError('stock firmware SHA-256 mismatch')
        self.elf = Path(elf)
        for line in self.run('nds32le-elf-nm', str(elf)).splitlines():
            parts = line.split()
            if len(parts) == 3:
                self.symbols[parts[2]] = int(parts[0], 16)
        self.headers = {}
        for line in self.run('nds32le-elf-objdump', '-h', str(elf)).splitlines():
            parts = line.split()
            if len(parts) == 7 and parts[0].isdigit():
                self.headers[parts[1]] = dict(size=int(parts[2],16), vma=int(parts[3],16), lma=int(parts[4],16))
        for p in self.patches:
            header = self.headers[p['section']]
            if header['vma'] != self.symbols[p['site']]:
                raise ValueError(f'{p["name"]}: linked section is at the wrong address')
            self.sections[p['section']] = self.extract(p['section'])
        allowed = {p['section'] for p in self.patches}
        for name, header in self.headers.items():
            if header['size'] and (name.startswith('.patch_') or name.endswith('hook')) and name not in allowed:
                raise ValueError(f'unlisted patch section: {name}')
        code = stock[0x60f:-4]
        for guard in self.guards:
            address = self.symbols[guard['site']]
            expected = self.originals[guard['site']]
            if len(expected) != guard['original_size'] or code[address:address+len(expected)] != expected:
                raise ValueError(f'native ABI guard failed: {guard["site"]}')
            self.guard_rows.append(dict(site=guard['site'],purpose=guard['purpose'],
                address=address,size=len(expected),instructions=self.disassemble(expected,address)))
        result = apply_patches(code, self.patches, self.symbols, self.sections, self.originals)
        for p in self.patches:
            address = self.symbols[p['site']]
            replacement = self.sections[p['section']]
            before = self.originals[p['site']]
            self.rows.append(dict(name=p['name'], purpose=p['purpose'], site=p['site'],
                address=address, max_size=p['max_size'], size=len(replacement), original_size=len(before),
                before_hex=before.hex(), after_hex=replacement.hex(),
                before=self.disassemble(before,address), after=self.disassemble(replacement,address)))
        return result

    def extract(self, section, elf=None):
        path = self.out/(section.lstrip('.')+'.bin')
        self.run('nds32le-elf-objcopy', '-O', 'binary', '-j', section, str(elf or self.elf), str(path))
        return path.read_bytes()

    def disassemble(self, data, address):
        path = self.out/'review-instructions.bin'
        path.write_bytes(data)
        text = self.run('nds32le-elf-objdump', '-D', '-b', 'binary', '-m', 'nds32', '-EL',
                        f'--adjust-vma={address}', str(path))
        names = {}
        for name, value in self.symbols.items():
            if value not in names or names[value].startswith('stock_patch_'):
                names[value] = name
        lines = [line.strip() for line in text.splitlines() if re.match(r'^\s*[0-9a-f]+:',line)]
        def annotate(match):
            address = int(match[0],16)
            return match[0]+(' <'+names[address]+'>' if address in names else '')
        return [re.sub(r'(?<!#)\b0x[0-9a-f]+\b',annotate,line) for line in lines]

    def report(self, image):
        memory = {name: header for name, header in self.headers.items() if name in ('.text','.data','.bss')}
        if '__bss_end' in self.symbols:
            memory['globals'] = dict(used=self.symbols['__bss_end']-self.symbols['__data_start'],
                                     limit=0x2004c000-self.symbols['__data_start'], remaining=0x2004c000-self.symbols['__bss_end'])
        if '.text' in self.headers:
            h = self.headers['.text']
            limit = self.headers.get('.data',{}).get('lma') if self.profile in ('app','runtime') else 0x1d0000
            memory['code'] = dict(remaining=limit-h['vma']-h['size'], limit_address=limit)
        report = dict(profile=self.profile, version=self.version, stock_sha256=self.manifest['stock_sha256'],
                      sha256=hashlib.sha256(image).hexdigest(), bytes=len(image), patches=self.rows, memory=memory,
                      guards=self.guard_rows, source_patches=self.source_patches)
        (self.out/'patch-report.json').write_text(json.dumps(report,indent=2)+'\n')
        lines = [f'# Firmware {self.version} patch review', '', f'SHA-256: `{report["sha256"]}`', '',
                 'All addresses below are decoded code addresses. The build verifies the stock image hash and every reserved overwrite byte.', '',
                 '## Memory', '', '```json', json.dumps(memory,indent=2), '```', '', '## Stock instruction edits', '']
        for row in self.rows:
            lines += [f'### {row["name"]}', '', row['purpose'], '',
                      f'`{row["site"]}` at `{row["address"]:#x}`; {row["size"]}/{row["max_size"]} bytes written/reserved; {row["original_size"]} stock bytes checked.', '',
                      'Before:', '', '```asm', *row['before'], '```', '', 'After:', '', '```asm', *row['after'], '```', '']
        lines += ['## Native ABI guards', '']
        for row in self.guard_rows:
            lines += [f'### {row["site"]}', '', row['purpose'], '',
                      '```asm', *row['instructions'], '```', '']
        lines += ['## Lua source patches', '']
        for name in self.source_patches:
            lines += [f'### {name}', '', '```diff',
                      (self.root/'native/patches'/name).read_text().rstrip(), '```', '']
        lines += ['## Container metadata', '',
                  'The builder also recalculates the application change marker, code CRC, package CRC, and (for extended images) length headers.',
                  'The stock bootloader executable is retained; its CRC must remain 0x5f08.', '']
        (self.out/'patch-report.md').write_text('\n'.join(lines))
        return report
