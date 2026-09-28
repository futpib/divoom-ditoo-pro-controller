"""The runner must not restore stock without both transfer and boot evidence."""
import json
from pathlib import Path
import runpy
import tempfile
import unittest

verified = runpy.run_path(str(Path(__file__).with_name('firmware-roundtrip.py')))['verified']


class VerificationGate(unittest.TestCase):
    def check_events(self, events):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'events.jsonl'
            path.write_text(''.join(json.dumps(event) + '\n' for event in events))
            return verified(path, 306008)

    def test_requires_transfer_and_boot(self):
        complete = {'event': 'device_update_complete', 'version': 306008}
        boot = {'event': 'verified', 'firmware_versions': [306008]}
        self.assertTrue(self.check_events([complete, boot]))
        self.assertFalse(self.check_events([complete]))
        self.assertFalse(self.check_events([boot]))
        self.assertFalse(self.check_events([]))

    def test_wrong_or_superseded_version_cannot_pass(self):
        complete = {'event': 'device_update_complete', 'version': 306008}
        wrong = {'event': 'verified', 'firmware_versions': [306007]}
        self.assertFalse(self.check_events([complete, wrong]))
        self.assertFalse(self.check_events([
            complete, {'event': 'verified', 'firmware_versions': [306008]}, wrong,
        ]))
        self.assertFalse(self.check_events([
            {'event': 'device_update_complete', 'version': 306007},
            {'event': 'verified', 'firmware_versions': [306008]},
        ]))


if __name__ == '__main__':
    unittest.main()
