import importlib.util
import json
from pathlib import Path
import re
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('manifest', ROOT/'scripts/firmware-manifest.py')
manifest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest)


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        for name in ['src/firmware.rs', 'src/lua.rs', 'src/usb_firmware.rs',
                     'scripts/check-lua-app-device.py', 'scripts/backup-lua-storage.py',
                     'scripts/build-reflash-probe.py', 'firmware/306007.MVA',
                     'firmware/306019-lua.MVA', 'firmware/306019-lua.json']:
            target = self.root/name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT/name, target)
        self.image = self.root/'firmware/306019-lua.MVA'
        self.meta = manifest.load_image(self.root, self.image)

    def test_check_and_update_are_idempotent(self):
        before, after = manifest.plan(self.root, self.image, self.meta)
        self.assertEqual(before, after)
        path = self.root/'src/firmware.rs'
        path.write_text(path.read_text().replace(self.meta['sha256'], '0'*64))
        before, after = manifest.plan(self.root, self.image, self.meta)
        self.assertNotEqual(before, after)
        self.assertIn('0'*64, path.read_text())  # planning is read-only
        manifest.apply(self.root, before, after)
        self.assertEqual(*manifest.plan(self.root, self.image, self.meta))
        path.write_text(path.read_text().replace('const USB_SIZE: usize = 2026511;', 'const USB_SIZE: usize = 2_026_511;'))
        self.assertEqual(*manifest.plan(self.root, self.image, self.meta))

    def test_named_patch_profile_must_match_image(self):
        folder = self.root/'native/patches'
        folder.mkdir(parents=True)
        spec = folder/'manifest.toml'
        spec.write_text('[profiles.app]\nversion = 306019\n')
        builder = self.root/'scripts/example-builder.py'
        builder.write_text("PATCH_PROFILE = 'app'\n")
        self.assertEqual(*manifest.plan(self.root,self.image,self.meta,builder=builder))
        spec.write_text('[profiles.app]\nversion = 306020\n')
        with self.assertRaisesRegex(ValueError,'profile version'):
            manifest.plan(self.root,self.image,self.meta,builder=builder)

    def test_concurrent_edit_is_preserved(self):
        name = 'src/firmware.rs'
        before = {name:(self.root/name).read_text()}
        after = {name:before[name]+'\n'}
        (self.root/name).write_text('concurrent change')
        with self.assertRaisesRegex(ValueError, 'changed during planning'):
            manifest.apply(self.root,before,after)
        self.assertEqual((self.root/name).read_text(), 'concurrent change')

    def test_report_and_package_corruption_fail_before_planning(self):
        data = bytearray(self.image.read_bytes())
        data[-10] ^= 1
        self.image.write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'report .* mismatch'):
            manifest.load_image(self.root,self.image)
        report = json.loads(self.image.with_suffix('.json').read_text())
        report['sha256'] = manifest.hashlib.sha256(data).hexdigest()
        report['checksum'] = sum(data)
        self.image.with_suffix('.json').write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, 'package CRC'):
            manifest.load_image(self.root,self.image)

    def test_registration_updates_all_allowlists(self):
        highest = max(map(int,re.findall(r'const \w*VERSION: u32 = (\d+);', (self.root/'src/firmware.rs').read_text())))
        version = highest + 1
        meta = dict(self.meta, version=version)
        image = self.image.with_name(f'{version}-lua.MVA')
        before, after = manifest.plan(self.root,image,meta,register='TEST_RUNTIME')
        self.assertIn('TEST_RUNTIME_SHA256 => TEST_RUNTIME_VERSION', after['src/firmware.rs'])
        restore = after['src/firmware.rs'].split('if restore_stock',1)[1].split('if installed /',1)[0]
        self.assertIn('TEST_RUNTIME_VERSION',restore)
        self.assertIn(f'validate_target({version+1}, VERSION, false, true).is_err()', after['src/firmware.rs'])
        self.assertIn('TEST_RUNTIME_VERSION',after['src/lua.rs'].split('if !matches!',1)[1])
        saved_error = 'Saved apps require firmware {STANDALONE_VERSION} or later; no program sent'
        self.assertIn(saved_error, after['src/lua.rs'])
        self.assertEqual(after['src/lua.rs'].count('{TEST_RUNTIME_VERSION}; no program sent'), 1)
        self.assertIn(f'"{version}-lua.MVA"',after['src/usb_firmware.rs'])
        self.assertIn(str(version),after['scripts/backup-lua-storage.py'].split('assert installed in',1)[1])
        manifest.apply(self.root,before,after)
        self.assertEqual(*manifest.plan(self.root,image,meta))


if __name__ == '__main__':
    unittest.main()
