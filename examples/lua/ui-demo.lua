local ui = require('../../lua/ui')
local view = ui.screen()
local started = time.millis()
return {
  init = function()
    brightness(20)
    view.set('DEMO', 'SHARED SCROLLING TEXT')
  end,
  key = function(_, event)
    if event == 1 then
      started = time.millis()
      view.set('DEMO', 'KEY PRESSED TIMER RESTARTED')
    end
  end,
  update = function()
    view.draw(nil, ui.elapsed(started) % 10000, 10000)
  end,
}
