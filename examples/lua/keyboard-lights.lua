-- Firmware 306014: chase the 12 physical LED positions; any key changes color.
local elapsed, index, selected = 0, 0, 1
local colors = {0x200000, 0x002000, 0x000020, 0x181818}
local function draw()
  lights.fill(0)
  lights.pixel(index, colors[selected])
  lights.present()
  display.clear(0)
  display.text(1, 1, 'LED', 0x20a0ff)
  display.text(4, 9, tostring(index), 0x40ff60)
  display.present()
end
return {
  init = function() brightness(20); draw() end,
  key = function(_, event)
    if event == 1 then selected = selected % #colors + 1; draw() end
  end,
  update = function(dt)
    elapsed = elapsed + dt
    if elapsed >= 500 then
      elapsed = 0
      index = (index + 1) % lights.count
      draw()
    end
  end,
}
