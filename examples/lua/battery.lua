-- Native coarse level and charger state, independent of a phone connection.
local elapsed = 1000
return {
  update = function(dt)
    elapsed = elapsed + dt
    if elapsed < 1000 then
      return
    end
    elapsed = 0
    local b = power.battery()
    display.clear(0)
    display.text(0, 0, 'B' .. b.level, 0x40a0ff)
    display.rect(1, 8, 14, 6, 0x404040, false)
    display.rect(2, 9, math.floor(b.level * 12 / 7), 4, 0x00a020, true)
    if b.charging then
      display.pixel(15, 0, 0xff8000)
    end
    if b.full then
      display.pixel(15, 0, 0x00ff00)
    end
    display.present()
  end,
}
