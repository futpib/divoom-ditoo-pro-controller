"""Check that the Lua configuration permits device APIs and catches real mistakes."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LuaToolConfiguration(unittest.TestCase):
    def lint(self, source):
        return subprocess.run(
            ['luacheck', '--config', str(ROOT/'.luacheckrc'), '--no-color',
             '--codes', '--filename', 'examples/lua/config-test.lua', '-'],
            input=source, text=True, capture_output=True, cwd=ROOT, check=False,
        )

    def test_device_apis_and_lua54(self):
        result = self.lint('''local ui = require('../../lua/ui')
local timestamp = time.millis()
return {update = function()
  local elapsed = (time.millis() - timestamp) & 0x7fffffff
  display.text(0, 0, tostring(elapsed // 1000), 0xffffff)
  ui.bar(7, elapsed, 1000)
  display.present()
end}
''')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_accidental_globals_and_api_typos(self):
        for source, diagnostic in [
            ('counter = 1', 'setting non-standard global'),
            ('return dispaly.clear(0)', "undefined variable 'dispaly'"),
            ('return display.tex(0, 0, "x", 0)', "undefined field 'tex'"),
            ('display.clear = function() end', 'read-only field'),
            ('local unused = 1; return 2', 'unused variable'),
        ]:
            with self.subTest(source=source):
                result = self.lint(source)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn(diagnostic, result.stdout)

    def test_unavailable_runtime_functions(self):
        for expression in ['arg', 'io.open', 'os.time', 'package.loaded', 'debug.traceback',
                           'utf8.len',
                           'load', 'loadfile', 'dofile', 'collectgarbage',
                           'string.format', 'string.dump', 'string.pack',
                           'string.unpack', 'string.packsize']:
            with self.subTest(expression=expression):
                result = self.lint('return '+expression)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn('undefined', result.stdout)

    def test_formatter_accepts_lua54_and_is_idempotent(self):
        source = 'local v <const> = (7 // 2) << 1\nreturn v & 255\n'
        args = ['stylua', '--config-path', str(ROOT/'.stylua.toml'), '--verify', '-']
        first = subprocess.run(args, input=source, text=True, capture_output=True, check=True)
        second = subprocess.run(args, input=first.stdout, text=True, capture_output=True, check=True)
        self.assertEqual(first.stdout, second.stdout)


if __name__ == '__main__':
    unittest.main()
