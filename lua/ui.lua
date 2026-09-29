-- Shared 16x16 UI. Bundled with the app by the host; no firmware module needed.
local ui = {}
function ui.elapsed(since, now)
  return ((now or time.millis()) - since) & 0x7fffffff
end
function ui.center(y, text, color)
  text = text:sub(1, 4)
  display.text((16 - #text * 4) // 2, y, text, color)
end
function ui.scroll(y, text, since, color, speed)
  if #text <= 4 then
    ui.center(y, text, color)
    return
  end
  local p = (ui.elapsed(since) // math.max(1, speed or 130)) % (#text * 4 + 24)
  local window = ('    ' .. text .. '  '):sub(p // 4 + 1, p // 4 + 5)
  display.text(-(p % 4), y, window, color)
end
function ui.bar(y, value, total, color, width)
  if total <= 0 then
    return
  end
  width = math.min(16, math.max(1, width or 15))
  local n = math.floor(math.max(0, math.min(1, value / total)) * width)
  if value > 0 then
    n = math.max(1, n)
  end
  if n > 0 then
    display.rect(0, y, n, 1, color or 0x2080ff, true)
  end
end
function ui.indicator(on, x, y)
  display.pixel(x or 15, y or 6, on and 0x20ff40 or 0x503010)
end
function ui.screen()
  local title, hint, color, since, last = '', '', 0x20a0ff, 0, 0
  return {
    set = function(t, h, c)
      if title ~= t or hint ~= h then
        title, hint, since = t, h, time.millis()
        app.log(t .. ': ' .. h)
      end
      color = c or 0x20a0ff
    end,
    draw = function(connected, value, total)
      if ui.elapsed(last) < 100 then
        return
      end
      last = time.millis()
      display.clear(0)
      ui.center(0, title, color)
      ui.scroll(10, hint, since, 0xb0b0b0)
      if connected ~= nil then
        ui.indicator(connected)
      end
      if value then
        ui.bar(7, value, total)
      end
      display.present()
    end,
  }
end
return ui
