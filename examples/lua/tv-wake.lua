-- One experimental Xiaomi RC wake burst for the TV saved by tv-keyboard.lua.
-- Use lua start, not install. The delay lets a BLE uploader disconnect.
local wake = require('../../lua/wake')
local ui = require('../../lua/ui')
local screen = ui.screen(true)
local ticket, finished
local function result(text, color)
  screen.set(text, '', color)
  app.log(text)
end
return {
  init = function()
    local peer = (storage.get() or ''):match('^TV[56]|([^|]+)|')
    if not peer then
      result('NO SAVED TV', 0xff4020)
      return
    end
    local data = wake.xiaomi_rc(peer)
    result('WAIT')
    timer.after(12000, function()
      local reason
      ticket, reason = bluetooth.advertise({
        data = data,
        type = 'connectable',
        interval_ms = 100,
        duration_ms = 3000,
      })
      result(ticket and 'WAKE' or reason, ticket and 0x20a0ff or 0xff4020)
    end)
  end,
  update = function()
    if ticket and not finished then
      local ok, reason = device.result(ticket)
      if ok ~= nil then
        finished = true
        result(ok and 'SENT' or reason, ok and 0x20ff40 or 0xff4020)
      end
    end
    screen.draw()
  end,
  key = function(id, event)
    if id == 0 and event == 1 then
      app.menu()
    end
  end,
}
