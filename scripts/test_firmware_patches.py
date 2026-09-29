"""Reject patch drift and unsafe overwrite sets before changing the stock image."""
import tempfile
import shutil
import subprocess
import tarfile
import unittest
from pathlib import Path

from firmware_patches import PatchSet, apply_patches, load_manifest

ROOT = Path(__file__).resolve().parents[1]


class PatchTests(unittest.TestCase):
    def setUp(self):
        self.code = bytes(range(32))
        self.patch = dict(name='example', site='site', section='.edit', max_size=4, original_size=4)
        self.symbols = {'site':4}
        self.sections = {'.edit':b'\xaa\xbb'}
        self.originals = {'site':self.code[4:8]}

    def apply(self, patches=None):
        return apply_patches(self.code, patches or [self.patch], self.symbols, self.sections, self.originals)

    def test_short_edit_preserves_tail_and_input(self):
        result = self.apply()
        self.assertEqual(result, self.code[:4]+b'\xaa\xbb'+self.code[6:])
        self.assertEqual(self.code, bytes(range(32)))

    def test_guard_covers_unmodified_tail(self):
        self.originals['site'] = b'\x04\x05\x06\xff'
        with self.assertRaisesRegex(ValueError, 'original bytes differ'):
            self.apply()

    def test_complete_instruction_guard_can_extend_past_overwrite_budget(self):
        self.patch['original_size'] = 6
        self.originals['site'] = self.code[4:10]
        self.assertEqual(self.apply()[6:], self.code[6:])
        self.originals['site'] = self.code[4:9]+b'\xff'
        with self.assertRaisesRegex(ValueError, 'original bytes differ'):
            self.apply()

    def test_original_assembly_must_have_the_declared_size(self):
        for data in [b'',self.code[4:7],self.code[4:9]]:
            self.originals['site'] = data
            with self.assertRaisesRegex(ValueError, 'original assembly size'):
                self.apply()

    def test_overlapping_reserved_regions_fail_even_for_short_edits(self):
        other = dict(self.patch, name='other', site='overlap')
        self.symbols['overlap'] = 6
        self.originals['overlap'] = self.code[6:10]
        with self.assertRaisesRegex(ValueError, 'overlapping patches'):
            self.apply([self.patch,other])

    def test_oversized_and_empty_edits_fail(self):
        for data in [b'',b'12345']:
            self.sections['.edit'] = data
            with self.assertRaisesRegex(ValueError, 'overwrite budget'):
                self.apply()

    def test_out_of_bounds_sites_fail(self):
        for address in [-1,30,32]:
            self.symbols['site'] = address
            with self.assertRaisesRegex(ValueError, 'outside stock'):
                self.apply()

    def test_a_later_invalid_patch_never_mutates_input(self):
        code = bytearray(self.code)
        other = dict(self.patch,name='bad',site='end')
        with self.assertRaises(ValueError):
            apply_patches(code,[self.patch,other],dict(self.symbols,end=99),self.sections,
                          dict(self.originals,end=self.originals['site']))
        self.assertEqual(code,self.code)

    def test_manifest_requires_complete_guards_and_unique_names(self):
        source = (ROOT/'native/patches/manifest.toml').read_text()
        for broken in [source.replace('original_size = 30','original_size = 26',1),
                       source.replace('name = "runtime_init"','name = "command_dispatch"',1),
                       source.replace('bnez38 $r0, stock_update_accept','.byte 0xc8, 0x16',1),
                       source.replace('bnez38 $r0, stock_update_accept','',1),
                       source.replace('lua/0001-bounded-runtime.patch','../outside.patch',1)]:
            with tempfile.TemporaryDirectory() as d:
                root = Path(d);(root/'native/patches').mkdir(parents=True)
                (root/'native/patches/manifest.toml').write_text(broken)
                with self.assertRaises(ValueError):
                    load_manifest(root)

    def test_profile_sections_are_unique(self):
        data = load_manifest(ROOT)
        patches = {p['name']:p for p in data['patches']}
        for profile in data['profiles'].values():
            sections = [patches[n]['section'] for n in profile['patches']]
            self.assertEqual(len(sections),len(set(sections)))


class ReadablePatchTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        shutil.copytree(ROOT/'native/patches',self.root/'native/patches')

    def lua_source(self):
        with tarfile.open(ROOT/'native/lua/vendor/lua-5.4.9.tar.gz') as tar:
            tar.extractall(self.root,filter='data')
        return self.root/'lua-5.4.9/src'

    def test_original_assembly_is_checked_against_vendor_bytes(self):
        manifest = self.root/'native/patches/manifest.toml'
        manifest.write_text(manifest.read_text().replace(
            'bnez38 $r0, stock_update_accept','j8 stock_update_accept'))
        patches = PatchSet(self.root,'reflash',self.root/'out')
        patches.prepare_originals()
        symbols = {parts[2]:int(parts[0],16)
                   for line in patches.run('nds32le-elf-nm',str(patches.out/'originals.elf')).splitlines()
                   if len(parts := line.split()) == 3}
        sections = {p['section']:patches.originals[p['site']] for p in patches.patches}
        stock = (ROOT/'firmware/306007.MVA').read_bytes()[0x60f:-4]
        with self.assertRaisesRegex(ValueError,'allow_reflash: original bytes differ'):
            apply_patches(stock,patches.patches,symbols,sections,patches.originals)

    def test_lua_diffs_apply_to_pinned_source(self):
        src = self.lua_source()
        patches = PatchSet(self.root,'app',self.root/'out')
        patches.prepare_sources(src)
        self.assertIn('#define LUA_32BITS\t1',(src/'luaconf.h').read_text())
        self.assertIn('init: runtime_poll();',(src/'lstrlib.c').read_text())
        self.assertNotIn('{"load", luaB_load}',(src/'lbaselib.c').read_text())
        with self.assertRaises(subprocess.CalledProcessError):
            patches.prepare_sources(src)

    def test_lua_diffs_reject_context_drift_without_fuzz(self):
        src = self.lua_source()
        config = src/'luaconf.h'
        original = config.read_text().replace(
            '@@ LUA_32BITS enables Lua with 32-bit integers and 32-bit floats.',
            '@@ Upstream changed the context.')
        config.write_text(original)
        patches = PatchSet(self.root,'runtime',self.root/'out')
        with self.assertRaises(subprocess.CalledProcessError):
            patches.prepare_sources(src)


if __name__ == '__main__':
    unittest.main()
