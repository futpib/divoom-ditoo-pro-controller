std = 'lua54'
max_line_length = 100
not_globals = {
  'arg',
  'io',
  'os',
  'package',
  'debug',
  'utf8',
  'load',
  'loadfile',
  'dofile',
  'collectgarbage',
  'string.format',
  'string.dump',
  'string.pack',
  'string.unpack',
  'string.packsize',
}
local function api(names)
  local fields = {}
  for name in names:gmatch('%S+') do
    fields[name] = {}
  end
  return { fields = fields }
end
read_globals = {
  display = api('clear pixel get line rect blit text present frame image glyph'),
  assets = api('count info image glyph'),
  time = api('millis calendar'),
  timer = api('after every cancel'),
  keys = api('held'),
  device = api('brightness volume stats result'),
  lights = api('count fill pixel frame enabled present claim'),
  app = api('claim stop menu log'),
  comms = api('send'),
  power = api('battery indicator get_schedule set_schedule'),
  alarm = api('get set status cancel snooze'),
  audio = api(
    'play pause source next previous direction track seek repeat_mode '
      .. 'preview stop status memo_play memo_delete'
  ),
  microphone = api('noise level record stop'),
  bluetooth = api('status connect_media disconnect_media media mute advertise advertise_cancel'),
  keyboard = api(
    'connect listen disconnect pair forget bonds name configure tap consumer media status mode'
  ),
  storage = api('get set'),
  'brightness',
  'volume',
}
