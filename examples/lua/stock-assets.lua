-- Any key advances the image. Messages: image 1, glyph 0416, sound 6, stop.
local image_id = 1
local ticket, playing_since, pending_since, stopping

local function show_image(id)
  assert(id >= 1 and id <= assets.count('image'), 'image ID out of range')
  image_id = id
  display.clear(0)
  assert(display.image(0, 0, id))
  display.present()
  app.log('image ' .. id .. '/' .. assets.count('image'))
end

local function stop_sound()
  ticket = assert(audio.stop())
  stopping = true
end

return {
  init = function()
    show_image(1)
  end,
  key = function(_, event)
    if event == 1 then
      show_image(image_id % assets.count('image') + 1)
    end
  end,
  message = function(command)
    local kind, value = command:match('^(%a+)%s+(%x+)$')
    if kind == 'image' then
      show_image(assert(tonumber(value)))
    elseif kind == 'glyph' then
      display.clear(0)
      assert(display.glyph(0, 0, assert(tonumber(value, 16)), 0x40c0ff))
      display.present()
      app.log('glyph U+' .. value)
    elseif kind == 'sound' and not pending_since then
      -- Explicit message only; browsing images never plays a sound.
      ticket = assert(audio.preview(assert(tonumber(value)), 10))
      pending_since = time.millis()
      playing_since, stopping = nil, false
    elseif command == 'stop' then
      stop_sound()
    else
      app.log('image ID | glyph HEX | sound 0..13 | stop')
    end
  end,
  update = function()
    if ticket then
      local ok, reason = device.result(ticket)
      if ok == nil then
        return
      end
      assert(ok, reason)
      ticket = nil
      if stopping then
        pending_since, playing_since, stopping = nil, nil, false
        app.log('sound stopped')
      end
    end
    if not pending_since then
      return
    end
    local now = time.millis()
    if not playing_since and audio.status().sound_playing then
      playing_since = now
    end
    if
      (playing_since and ((now - playing_since) & 0x7fffffff) >= 300)
      or ((now - pending_since) & 0x7fffffff) >= 2000
    then
      stop_sound()
    end
  end,
}
