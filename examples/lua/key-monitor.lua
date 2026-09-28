return {
  init = function()
    brightness(20)
    display.clear(0)
    display.text(0, 5, 'KEY', 0x20a0ff)
    display.present()
  end,
  key = function(key, event)
    local message = key .. ':' .. event
    app.log(message)
    comms.send(message)
    display.clear(0)
    display.text(0, 5, message, 0x40ff60)
    display.present()
  end,
  update = function() end,
}
