"""Reject patch drift and unsafe overwrite sets before changing the stock image."""
import tempfile
import unittest
from pathlib import Path

from firmware_patches import apply_patches, load_manifest

ROOT = Path(__file__).resolve().parents[1]


class PatchTests(unittest.TestCase):
    def setUp(self):
        self.code = bytes(range(32))
        self.patch = dict(name='example', site='site', section='.edit', max_size=4, expected='04050607')
        self.symbols = {'site':4}
        self.sections = {'.edit':b'\xaa\xbb'}

    def apply(self, patches=None):
        return apply_patches(self.code, patches or [self.patch], self.symbols, self.sections)

    def test_short_edit_preserves_tail_and_input(self):
        result = self.apply()
        self.assertEqual(result, self.code[:4]+b'\xaa\xbb'+self.code[6:])
        self.assertEqual(self.code, bytes(range(32)))

    def test_guard_covers_unmodified_tail(self):
        self.patch['expected'] = '040506ff'
        with self.assertRaisesRegex(ValueError, 'original bytes differ'):
            self.apply()

    def test_overlapping_reserved_regions_fail_even_for_short_edits(self):
        other = dict(self.patch, name='other', site='overlap', expected='06070809')
        self.symbols['overlap'] = 6
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
            apply_patches(code,[self.patch,other],dict(self.symbols,end=99),self.sections)
        self.assertEqual(code,self.code)

    def test_manifest_requires_complete_guards_and_unique_names(self):
        source = (ROOT/'native/patches/manifest.toml').read_text()
        for broken in [source.replace('max_size = 28','max_size = 4',1),
                       source.replace('name = "runtime_init"','name = "command_dispatch"',1)]:
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


if __name__ == '__main__':
    unittest.main()
