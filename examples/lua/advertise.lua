-- Harmless manufacturer-data burst; this is not a TV wake packet.
-- Start over USB, or close the BLE control connection during the delay.
local ticket, finished
return {
  init = function()
    timer.after(5000, function()
      local reason
      ticket, reason = bluetooth.advertise({
        data = string.char(8, 255, 255, 255) .. 'DITOO',
        type = 'nonconnectable',
        interval_ms = 100,
        duration_ms = 1500,
      })
      if not ticket then
        app.log(reason)
        finished = true
      end
    end)
  end,
  update = function()
    if ticket and not finished then
      local ok, reason = device.result(ticket)
      if ok ~= nil then
        app.log(ok and 'advertising complete' or reason)
        finished = true
      end
    end
  end,
  message = function(message)
    if message == 'cancel' and ticket then
      bluetooth.advertise_cancel(ticket)
    end
  end,
}
