local last = -1
local colors = {0x20a0ff, 0xffa040, 0xff60a0, 0x40ff60}
local selected = 1
local function two(n) return n < 10 and '0' .. n or tostring(n) end
return {
  init = function() brightness(20) end,
  key = function(key, event)
    if event == 1 then
      selected = selected % #colors + 1
      last = -1
      app.log('key ' .. key .. ': color ' .. selected)
    end
  end,
  update = function()
    local t = time.calendar()
    if t.sec == last then return end
    last = t.sec
    display.clear(0)
    display.text(0, 1, two(t.hour), colors[selected])
    display.text(9, 1, two(t.min), colors[selected])
    display.text(4, 9, two(t.sec), 0x40ff60)
    display.present()
  end,
  message = function(s) app.log(s) end,
}
