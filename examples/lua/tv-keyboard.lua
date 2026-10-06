local ui = require('../../lua/ui')
local view = ui.screen(true)
local screen, age, now = view.set, ui.elapsed, time.millis
-- Stock ADC IDs: lever, source, sun, M, +, -, left, right.
local keys = { [4] = 1, [10] = 2, [7] = 3, [0] = 4, [1] = 5, [9] = 6, [2] = 7, [3] = 8 }
local actions =
  { 'play_pause', 'mute', 'space', false, 'volume_up', 'volume_down', 'left', 'right' }
local labels = { 'PLAY/PAUSE', 'MUTE', 'SPACE', '', 'VOLUME UP', 'VOLUME DOWN', 'LEFT', 'RIGHT' }
local icons = { '>II', 'X', '_', 'M', '+', '-', '<', '>' }
local taps = { space = 44, left = 80, right = 79 }
local b, bonds = {}, {}
local peer, kind, on = nil, 'public', false
local job, flow, pair, err, key, icon
local step, at, retry, key_at, sent_at, shown = 0, 0, 0, 0, 0, 0
local dirty, saving, save_at, errors = false, nil, 0, 0
local page, pos, item = nil, 1, nil
local root = { 'DEVICES', 'PAIR NEW DEVICE', 'EXIT', 'BACK' }
local function same(address, addr_type)
  return peer == address and kind == addr_type
end
local function label(address, addr_type)
  return address and (address .. ' ' .. addr_type:upper()) or 'NO SAVED DEVICE'
end
local function dirty_settings()
  dirty, save_at = true, now()
end
local function config()
  return 'TV6|' .. (peer or '-') .. '|' .. kind .. '|' .. (on and '1' or '0')
end
local function queue(name, fn, ...)
  local ticket, reason = fn(...)
  if ticket then
    job = { id = ticket, name = name }
    return true
  elseif reason ~= 'busy' then
    err = 'PLEASE TRY AGAIN'
  end
end
local function begin(action)
  if flow then
    return
  end
  flow, step, at = action, 1, now()
  page, key, icon, err, pair = nil, nil, nil, nil, nil
  if action ~= 'forget' then
    on = action == 'connect'
    dirty_settings()
  end
end
local function open(p)
  page, pos, key, icon = p, 1, nil, nil
  if p == 'devices' then
    bonds = b.transport == 'ble' and keyboard.bonds(true) or {}
  end
end
local function active()
  return item
    and (
      (b.connected and b.peer == item.address and b.address_type == item.address_type)
      or (on and same(item.address, item.address_type))
    )
end
local function select()
  if flow then
    return
  end
  if page == 'root' then
    if pos == 1 then
      open('devices')
    elseif pos == 2 then
      begin((pair or b.pairing) and 'disconnect' or 'pair')
    elseif pos == 3 then
      app.menu()
    else
      page = nil
    end
  elseif page == 'devices' then
    item = bonds[pos]
    if item then
      open('device')
    else
      open('root')
    end
  elseif page == 'device' then
    if pos == 1 then
      if active() then
        begin('disconnect')
      else
        peer, kind = item.address, item.address_type
        begin('connect')
      end
    elseif pos == 2 then
      open('forget')
    else
      open('devices')
    end
  elseif pos == 1 then
    open('device')
  else
    begin('forget')
  end
end
local function count()
  return page == 'root' and 4 or page == 'devices' and (#bonds + 1) or page == 'device' and 3 or 2
end
local function press(role)
  if flow or pair or b.transport ~= 'ble' then
    return
  end
  if not b.connected then
    if on and peer then
      key, key_at = role, now()
    end
    return
  end
  if job or b.busy or age(sent_at) < 200 then
    key, key_at = role, now()
    return
  end
  local action = actions[role]
  if queue('key', taps[action] and keyboard.tap or keyboard.media, taps[action] or action) then
    icon, shown, sent_at = role, now(), now()
  end
end
local function progress()
  if not flow or job then
    return
  end
  if age(at) > 15000 then
    err, flow, on = err or 'COULD NOT COMPLETE', nil, false
    dirty_settings()
  elseif b.transport ~= 'ble' then
    if b.state ~= 0 then
      queue('mode', keyboard.disconnect)
    elseif not b.mode_locked then
      queue('mode', keyboard.mode, 'ble-remote')
    else
      err = 'SYSTEM BT - APP OR REMOTE'
    end
  elseif step == 1 then
    if flow == 'connect' and b.connected and same(b.peer, b.address_type) then
      flow = nil
    else
      queue('step', audio.source, 'bluetooth')
    end
  elseif step == 2 then
    if b.state == 0 then
      step = 4
    else
      queue('step', keyboard.disconnect)
    end
  elseif step == 3 then
    if b.state == 0 then
      step = 4
    end
  elseif step == 4 and age(at) > 2000 then
    if flow == 'disconnect' then
      flow = nil
    elseif flow == 'forget' then
      queue('step', keyboard.forget, item.address, item.address_type)
    elseif flow == 'pair' then
      if queue('pair', keyboard.pair) then
        step = 5
      end
    elseif peer and queue('done', keyboard.listen, peer, kind) then
      flow, retry = nil, now()
    end
  elseif step == 5 then
    if flow == 'pair' then
      if b.pairing or b.connected then
        flow, pair = nil, true
      end
    else
      for _, bond in ipairs(keyboard.bonds(true)) do
        if bond.address == item.address and bond.address_type == item.address_type then
          return
        end
      end
      if same(item.address, item.address_type) then
        peer, on = nil, false
        dirty_settings()
      end
      flow = nil
      open('devices')
    end
  end
end
return {
  init = function()
    assert(keyboard.status().transport, 'firmware 306029 required')
    brightness(20)
    lights.fill(0)
    lights.present()
    local version, address, addr_type, enabled = (storage.get() or ''):match(
      '^TV([56])|([^|]+)|([^|]+)|(.*)'
    )
    if
      (addr_type == 'public' or addr_type == 'random')
      and address:match('^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$')
    then
      peer, kind, on = address, addr_type, version == '5' or enabled == '1'
    end
    keyboard.status(b)
    errors = b.errors
    if not version and b.connected and b.transport == 'ble' then
      peer, kind, on = b.peer, b.address_type, true
      dirty_settings()
    end
    if on then
      begin('connect')
    elseif b.connected or b.pairing or b.transport ~= 'ble' then
      begin('disconnect')
    end
    if not peer then
      open('root')
      pos = 2
    end
  end,
  key = function(k, event)
    local role = keys[k]
    if event ~= 1 or not role then
      return
    end
    if role == 4 then
      if page then
        page = nil
      else
        open('root')
      end
    elseif page then
      if role == 7 or role == 8 then
        pos = (pos - 1 + (role == 7 and -1 or 1)) % count() + 1
      elseif role == 1 then
        select()
      elseif role == 2 then
        if page == 'root' then
          page = nil
        else
          open(page == 'forget' and 'device' or page == 'device' and 'devices' or 'root')
        end
      end
    elseif role == 2 and (pair or b.pairing) then
      begin('disconnect')
    elseif not peer and not b.connected then
      open('root')
      pos = 2
    else
      press(role)
    end
  end,
  message = function(s)
    if s == 'toggle' then
      s = 'play_pause'
    end
    if s == 'pair' or s == 'disconnect' then
      begin(s)
    elseif s == 'connect' or s == 'listen' then
      if peer then
        begin('connect')
      else
        open('root')
        pos = 2
      end
    elseif s == 'menu' then
      if page then
        page = nil
      else
        open('root')
      end
    elseif s == 'status' then
      comms.send(b.state .. ' ' .. (b.peer or '-') .. ' ' .. (b.address_type or '-'))
    else
      local address = s:match('^target (%x%x:%x%x:%x%x:%x%x:%x%x:%x%x)$')
      if address and not flow then
        peer, kind = address:upper(), 'public'
        dirty_settings()
      else
        for i, action in ipairs(actions) do
          if s == action then
            press(i)
          end
        end
      end
    end
  end,
  update = function()
    keyboard.status(b)
    if job then
      local ok = device.result(job.id)
      if ok ~= nil then
        local name = job.name
        job = nil
        if not ok then
          err = name == 'save' and 'COULD NOT SAVE' or 'PLEASE TRY AGAIN'
          flow, key = nil, nil
          save_at = now()
        elseif name == 'save' then
          dirty = config() ~= saving
          if not dirty and err == 'COULD NOT SAVE' then
            err = nil
          end
        elseif name == 'step' then
          step = step + 1
        end
      end
    end
    progress()
    if pair and b.connected then
      peer, kind, on, pair, err = b.peer, b.address_type, true, nil, nil
      dirty_settings()
    elseif pair and not b.pairing then
      pair, on, err = nil, false, 'PAIRING TIMED OUT'
      begin('disconnect')
      err = 'PAIRING TIMED OUT'
    end
    if b.connected and (err == 'COULD NOT CONNECT' or err == 'COULD NOT PAIR') then
      err = nil
    end
    if b.errors ~= errors then
      errors, key = b.errors, nil
      err = pair and 'COULD NOT PAIR' or 'COULD NOT CONNECT'
    end
    if key and age(key_at) > 5000 then
      key = nil
    end
    if key and b.connected and not job and not b.busy and age(sent_at) >= 200 then
      local role = key
      key = nil
      press(role)
    end
    if
      not flow
      and not job
      and not pair
      and on
      and peer
      and b.transport == 'ble'
      and b.state == 0
      and age(retry) > 1000
    then
      queue('done', keyboard.listen, peer, kind)
      retry = now()
    end
    if dirty and not flow and not job and age(save_at) > 1200 then
      saving = config()
      queue('save', storage.set, saving)
    end
    if icon and age(shown) > 700 then
      icon = nil
    end
    local title, hint = 'DISCONNECTED', label(peer, kind)
    if page == 'root' then
      title, hint = root[pos], 'REMOTE'
      if pos == 2 and (pair or b.pairing) then
        title = 'CANCEL PAIRING'
      end
    elseif page == 'devices' then
      local bond = bonds[pos]
      title, hint =
        bond and 'SAVED DEVICE' or 'BACK',
        bond and label(bond.address, bond.address_type)
          or (#bonds == 0 and 'NO SAVED DEVICES' or 'DEVICES')
    elseif page == 'device' or page == 'forget' then
      hint = label(item.address, item.address_type)
      if page == 'forget' then
        title, hint = pos == 1 and 'CANCEL' or 'FORGET', 'FORGET ' .. hint .. '?'
      else
        title = pos == 1 and (active() and 'DISCONNECT' or 'CONNECT')
          or pos == 2 and 'FORGET'
          or 'BACK'
      end
    elseif err then
      title, hint = 'ATTENTION', err
    elseif flow then
      title = flow == 'pair' and 'SETTING UP'
        or flow == 'forget' and 'FORGETTING'
        or flow == 'disconnect' and 'DISCONNECTING'
        or 'CONNECTING'
    elseif b.connected then
      title, hint =
        icon and icons[icon] or 'CONNECTED', icon and labels[icon] or label(b.peer, b.address_type)
    elseif b.state == 1 then
      title = pair and not b.paired and 'PAIRING' or 'CONNECTING'
    elseif b.pairing then
      title, hint = 'READY TO PAIR', 'SELECT DITOO BLE REMOTE'
    elseif on and peer then
      title = 'WAITING FOR DEVICE'
    end
    screen(title, hint, b.connected and 0x20ff40 or err and 0xffa030 or nil)
    view.draw(
      b.connected,
      page and pos or b.pairing and b.pair_remaining_ms or nil,
      page and count() or 120000
    )
  end,
}
