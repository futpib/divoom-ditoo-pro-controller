#!/usr/bin/env python3
"""Exercise native audio and recorder recovery on 306016.

--record-memo explicitly enables a short local microphone recording, playback,
deletion, and another recording interrupted by an infinite Lua loop. The native
memo slot is replaced. No audio bytes are transferred to the host.
"""
import argparse
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('peripherals', ROOT/'scripts/check-lua-peripherals.py')
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('device')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--record-memo', action='store_true', required=True)
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
d = checks.Device(a.device, a.output, a.binary)

source = '''local ticket
return {
 init=function() app.log(device.stats().free_heap) end,
 update=function()
  if not ticket then return end
  local ok,e=device.result(ticket)
  if ok~=nil then app.log(tostring(ok)..':'..tostring(e));ticket=nil end
 end,
 message=function(s)
  if s=='record' then ticket=assert(microphone.record())
  elseif s=='stop' then ticket=assert(microphone.stop())
  elseif s=='play' then ticket=assert(audio.memo_play())
  elseif s=='delete' then ticket=assert(audio.memo_delete())
  elseif s=='noise' then ticket=assert(microphone.noise(true))
  elseif s=='level' then local n=microphone.level();app.log(tostring(n))
  elseif s=='status' then local b=audio.status();
   app.log(table.concat({tostring(b.recording),tostring(b.sound_playing),b.recorded_bytes,device.stats().free_heap},':'))
  elseif s=='crash' then while true do end end
 end
}'''


def capture(name):
    d.checks.append((len(d.requests), name))
    d.op(0)


def message(text, delay=500, name=None):
    d.op(8, text.encode(), delay)
    if name:
        capture(name)


d.case('loaded', source)
message('record', 2000, 'record-start')
message('status', name='recording')
message('stop', 1500, 'record-stop')
message('play', 800, 'play-start')
message('status', name='playing')
d.op(0, delay=8000)
message('status', name='play-finished')
message('delete', name='deleted')
normal = d.run('memo')
assert normal['record-start'] == normal['record-stop'] == normal['play-start'] == 'true:nil'
recording = normal['recording'].split(':')
assert recording[0] == 'true' and int(recording[2]) > 0
assert normal['playing'].split(':')[1] == 'true'
assert normal['play-finished'].split(':')[:2] == ['false', 'false']
assert normal['deleted'] == 'true:nil'

d.case('reloaded', source)
message('record', 1200, 'record-start')
message('status', name='recording')
message('crash', 1000)
error_index = len(d.requests)
d.op(0)
d.case('recovery', 'return {init=function() app.log(device.stats().free_heap) end}')
recovery = d.run('record-loop')
rows = [json.loads(line) for line in (a.output/'record-loop-replies.jsonl').read_text().splitlines()]
error = bytes.fromhex(rows[error_index]['response']['data_hex'])
assert error[:7] == b'DLUA\x02\x03\x00' and int.from_bytes(error[16:20], 'little') == 0
assert recovery['record-start'] == 'true:nil' and recovery['recording'].startswith('true:')
assert int(recovery['recovery']) > 50000

d.case('cleanup', source)
message('delete', name='deleted-after-error')
message('noise', name='noise-start')
message('level', 500, 'noise-level')
message('crash', 500)
noise_error_index = len(d.requests)
d.op(0)
d.case('recovery', 'return {init=function() app.log(device.stats().free_heap) end}')
noise = d.run('noise-loop')
rows = [json.loads(line) for line in (a.output/'noise-loop-replies.jsonl').read_text().splitlines()]
error = bytes.fromhex(rows[noise_error_index]['response']['data_hex'])
assert error[:7] == b'DLUA\x02\x03\x00' and int.from_bytes(error[16:20], 'little') == 0
assert noise['noise-start'] == 'true:nil' and 0 <= int(noise['noise-level']) <= 150
assert int(noise['recovery']) > 50000
(a.output/'results.json').write_text(json.dumps({'memo': normal, 'record_loop': recovery,
                                               'noise_loop': noise}, indent=2)+'\n')
