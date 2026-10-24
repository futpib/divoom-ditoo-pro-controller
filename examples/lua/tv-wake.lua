-- Wake compatible Xiaomi TVs without pairing. Sun wakes the saved TV; messages
-- may supply up to six public TV addresses: wake AA:BB:CC:DD:EE:FF ...
-- Requests wait twelve seconds so a BLE uploader can disconnect. No settings
-- or bonds are changed, and each target gets one bounded advertising burst.
local wake = require('../../lua/wake')
local ui = require('../../lua/ui')
local screen = ui.screen(true)
local targets, index, at, ticket
local function result(text, color)
  screen.set(text, '', color)
  app.log(text)
end
local function request(addresses)
  if targets or ticket then
    return
  end
  if not addresses or addresses == '' then
    addresses = (storage.get() or ''):match('^TV[56]|([^|]+)|public|') or ''
  end
  local packets, seen = {}, {}
  for address in addresses:gmatch('%S+') do
    local ok, data = pcall(wake.xiaomi_app, address)
    if not ok or (not seen[data] and #packets == 6) then
      result('INVALID TARGETS', 0xff4020)
      return
    end
    if not seen[data] then
      packets[#packets + 1], seen[data] = data, true
    end
  end
  if #packets == 0 then
    result('SELECT A TV', 0xff4020)
    return
  end
  targets, index, at = packets, 1, time.millis()
  result('WAIT')
end
return {
  init = function()
    assert(bluetooth.advertise, 'firmware 306040 required')
    result('READY')
  end,
  message = function(message)
    if message == 'wake' then
      request()
    elseif message:sub(1, 5) == 'wake ' then
      request(message:sub(6))
    elseif message == 'cancel' then
      targets = nil
      if ticket then
        bluetooth.advertise_cancel(ticket)
      else
        result('CANCELLED')
      end
    end
  end,
  update = function()
    if ticket then
      local ok, reason = device.result(ticket)
      if ok ~= nil then
        ticket = nil
        if not targets then
          result('CANCELLED')
        elseif not ok then
          targets = nil
          result(reason, 0xff4020)
        else
          index = index + 1
          if index > #targets then
            targets = nil
            result('WAKE SENT', 0x20ff40)
          end
        end
      end
    end
    if targets and not ticket and ui.elapsed(at) >= 12000 then
      local reason
      ticket, reason = bluetooth.advertise({
        data = targets[index],
        type = 'nonconnectable',
        interval_ms = 100,
        duration_ms = 3000,
      })
      if ticket then
        result('SENDING WAKE', 0x20a0ff)
      else
        targets = nil
        result(reason, 0xff4020)
      end
    end
    screen.draw()
  end,
  key = function(id, event)
    if event == 1 then
      if id == 7 then
        request()
      elseif id == 0 then
        app.menu()
      end
    end
  end,
}
