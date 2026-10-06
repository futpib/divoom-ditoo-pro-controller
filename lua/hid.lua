-- Names and profiles are app data. Firmware validates and releases each report.
local hid = {}
function hid.media(action, duration)
  local usage = ({
    play_pause = 205,
    mute = 226,
    volume_up = 233,
    volume_down = 234,
    next = 181,
    previous = 182,
    stop = 183,
    power = 48,
    home = 547,
    back = 548,
    menu = 64,
  })[action]
  return keyboard.consumer(assert(usage, 'unknown media action'), duration)
end
function hid.remote()
  return keyboard.configure({
    name = 'Ditoo BLE Remote',
    appearance = 384,
    wake = true,
    keys = { 40, 41, 44, 79, 80, 81, 82 },
    consumer = { 205, 226, 233, 234, 181, 182, 183, 48, 547, 548, 64 },
  })
end
function hid.repeat_media(action, interval, duration)
  assert(interval >= (duration or 80) + 100, 'repeat interval is too short')
  return timer.every(interval, function()
    hid.media(action, duration)
  end)
end
return hid
