import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('inspector', Path(__file__).with_name('fw-inspect.py'))
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)


class InspectionTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.code = Path(self.tmp.name) / 'code.bin'
        self.code.write_bytes(b'aaa\0' + (0x1000).to_bytes(4, 'little') + bytes(56))
        self.asm = self.code.with_suffix('.nds32.S')
        self.asm.write_text(''' 1000: 46 00 00 01 sethi $r0, #1
 1004: 58 00 80 00 ori $r0, $r0, #0x234
 1008: 49 00 00 00 jal 0x1234
 100c: 58 00 80 00 ori $r0, $r0, #0x234
 1010: 46 10 00 01 sethi $r1, #1
 1014: 84 20 movi55 $r1, #0
 1016: 58 10 80 00 ori $r1, $r1, #0x234
 101a: 3c 0d e5 60 lwi.gp $r0, [ + #-27264]
''')

    def test_bounds_overlaps_and_base(self):
        f = inspector.Firmware(self.code, base=0x1000)
        self.assertEqual(list(f.find(b'aa')), [0x1000, 0x1001])
        self.assertEqual(f.read(0x1003, 1), b'\0')
        for address, size in [(0xfff, 1), (0x103f, 2), (0x1000, -1)]:
            with self.assertRaises(ValueError):
                f.read(address, size)
        with self.assertRaises(ValueError):
            list(f.find(b''))

    def test_references_do_not_cross_calls_or_overwrites(self):
        f = inspector.Firmware(self.code, base=0x1000)
        self.assertEqual([r['address'] for r in f.references(0x1234)], [0x1004])
        self.assertEqual([r['kind'] for r in f.references(f.gp - 27264)], ['gp_memory'])

    def test_table_and_callers(self):
        args = ['--code', str(self.code), '--base', '0x1000']
        p = inspector.parser()
        rows = inspector.inspect(p.parse_args(args + ['table', '0x1004', '--count', '1', '--strings']))
        self.assertEqual(rows[0]['string'], 'aaa')
        rows = inspector.inspect(p.parse_args(args + ['callers', '0x1234']))
        self.assertEqual(rows[0]['address'], 0x1008)
        with self.assertRaises(ValueError):
            inspector.inspect(p.parse_args(args + ['table', '0x1004', '--count', '65537']))


if __name__ == '__main__':
    unittest.main()
