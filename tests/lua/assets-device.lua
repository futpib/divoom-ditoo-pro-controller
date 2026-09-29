-- Run by check-lua-assets.py; reads stock resources without changing saved state.
local id, hash, finished = 1, 0x811c9dc5, false
local hex = '0123456789abcdef'
return {
  init = function()
    assert(assets.count('image') == 598 and assets.count('sound') == 14)
    for mode = 0, 13 do
      assert(assets.info('sound', mode).id == mode)
    end
    assert(not pcall(assets.image, 0) and not pcall(assets.image, 599))
    assert(not assets.glyph(0) and not assets.glyph(0xd800))
  end,
  update = function()
    if finished then
      return
    end
    local pixels = assert(assets.image(id))
    assert(#pixels == 768)
    assert(display.image(0, 0, id))
    assert(display.frame() == pixels)
    display.present()
    for i = 1, #pixels do
      hash = (hash ~ pixels:byte(i)) * 0x01000193
    end
    if id == assets.count('image') then
      finished = true
      app.log('images ' .. id .. ' ' .. hash)
    end
    id = id + 1
  end,
  message = function(message)
    local cp = assert(tonumber(message))
    local glyph = assert(assets.glyph(cp))
    assert(#glyph == 32)
    display.clear(0)
    assert(display.glyph(0, 0, cp, 0x40c0ff))
    for x = 0, 15 do
      local column = glyph:byte(2 * x + 1) | (glyph:byte(2 * x + 2) << 8)
      for y = 0, 15 do
        assert(display.get(x, y) == ((column & (1 << y)) ~= 0 and 0x40c0ff or 0))
      end
    end
    display.present()
    local out = ''
    for i = 1, #glyph do
      local b = glyph:byte(i)
      out = out .. hex:sub((b >> 4) + 1, (b >> 4) + 1) .. hex:sub((b & 15) + 1, (b & 15) + 1)
    end
    app.log('glyph ' .. cp .. ' ' .. out)
  end,
}
