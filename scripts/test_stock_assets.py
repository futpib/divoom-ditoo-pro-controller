import importlib.util
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('stock_assets', ROOT/'scripts/stock-assets.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


class StockAssetTests(unittest.TestCase):
    def test_catalogue_is_pinned_and_generated_files_match(self):
        code = assets.stock_code()
        frames = assets.frames(code)
        self.assertEqual(len(frames), 598)
        self.assertEqual(frames[0]['address'], 0x15f654)
        self.assertEqual(frames[-1]['address'], 0x17ffb4)
        self.assertEqual(frames[-1]['pixels'], bytes(768))
        self.assertEqual([f['id'] for f in frames], list(range(1, 599)))
        for name, content in assets.outputs(code).items():
            self.assertEqual((ROOT/name).read_text(), content, name)

    def test_palette_indices_are_lsb_first(self):
        frame = b'\xaa'+struct.pack('<HHBB', 45, 100, 0, 2)
        frame += bytes([255, 0, 0, 0, 255, 0])+b'\x01'+bytes(31)
        pixels = assets.decode(frame)['pixels']
        self.assertEqual(pixels[:6], bytes([0, 255, 0, 255, 0, 0]))
        for n in [0, 6, 7, 44]:
            with self.assertRaises(ValueError):
                assets.decode(frame[:n])
        bad = bytearray(frame)
        bad[5] = 1
        with self.assertRaises(ValueError):
            assets.decode(bad)

    def test_palette_size_edges_and_invalid_indices(self):
        one = b'\xaa'+struct.pack('<HHBB', 10, 500, 0, 1)+b'\x12\x34\x56'
        self.assertEqual(assets.decode(one)['pixels'], b'\x12\x34\x56'*256)
        full = b'\xaa'+struct.pack('<HHBB', 1031, 100, 0, 0)+bytes(768)+bytes(range(256))
        self.assertEqual(assets.decode(full)['colors'], 256)
        self.assertEqual(assets.decode(full)['pixels'], bytes(768))
        bad = b'\xaa'+struct.pack('<HHBB', 80, 100, 0, 3)+bytes(9)+b'\xff'*64
        with self.assertRaisesRegex(ValueError, 'palette index'):
            assets.decode(bad)


if __name__ == '__main__':
    unittest.main()
