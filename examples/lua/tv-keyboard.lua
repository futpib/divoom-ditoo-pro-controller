local ui = require('../../lua/ui')
local view = ui.screen()
local screen, age = view.set, ui.elapsed
-- Firmware 306025. Install once with `lua install`; setup then uses only keys.
local keys = {}
local target, pending
local b = {}
local job, flow
local stage, stage_at = 0, 0
local save_at, retries, retry_at = 0, 0, 0
local dirty, saving
local shown = 0
local menu, confirm, notice
local key_at, errors, linked = 0, 0
local online = true
local choices = { 'LINK', 'PAIR', 'KEYS', 'OFF', 'BACK' }
local labels = { 'PLAY', 'MUTE', 'SPC', 'MENU', 'VOL+', 'VOL-', 'LEFT', 'RGHT' }
local actions =
  { 'play_pause', 'mute', 'space', false, 'volume_up', 'volume_down', 'left', 'right' }
local icons = { '>II', 'X', '_', 'M', '+', '-', '<', '>' }
local taps = { space = 44, left = 80, right = 79 }
local now = time.millis
local function changed()
  dirty = true
  save_at = now()
end
local function config()
  return 'TV3|' .. (target or '-') .. '|' .. table.concat(keys, ',')
end
local function hint(s)
  notice = s
  shown = now()
end
local function queue(name, fn, ...)
  local t, err = fn(...)
  if t then
    job = { id = t, name = name }
    return true
  end
  if err ~= 'busy' then
    hint('WAIT THEN RETRY')
  end
  return false
end
local function begin(kind)
  if job then
    hint('PLEASE WAIT')
    return
  end
  menu = nil
  confirm = false
  notice = nil
  pending = nil
  flow = kind
  online = kind ~= 'off'
  stage = 1
  retries = 0
end
local function press(role)
  keyboard.status(b)
  if not b.connected then
    hint('OFFLINE TAP MENU')
    return
  end
  if job or b.busy or age(key_at) < 200 then
    pending = role
    return
  end
  key_at = now()
  local action = actions[role]
  if queue('key', taps[action] and keyboard.tap or keyboard.media, taps[action] or action) then
    hint(role)
  end
end
local function select()
  local s = choices[menu]
  if s == 'PAIR' then
    confirm = true
    menu = nil
  elseif s == 'KEYS' then
    keys = {}
    menu = nil
    changed()
  elseif s == 'BACK' then
    menu = nil
  else
    begin(s == 'OFF' and 'off' or 'connect')
  end
end
local function connect()
  if not b.keyboard_only and not job and (b.enabled or not flow or stage > 1) then
    queue('mode', keyboard.mode, 'keyboard')
    return
  end
  if flow and not job then
    if stage == 1 then
      if flow == 'connect' and b.connected and b.peer == target then
        flow = nil
      else
        queue('step', audio.source, 'bluetooth')
      end
    elseif stage < 4 then
      if b.state == 0 or (flow == 'connect' and b.state == 4 and b.peer == target) then
        if flow == 'off' then
          flow = nil
          hint('OFFLINE TAP MENU')
        else
          stage = 4
          stage_at = now()
        end
      elseif stage == 2 then
        queue('step', keyboard.disconnect)
      end
    elseif stage == 4 and age(stage_at) > 2000 then
      if flow == 'pair' and target then
        queue('step', keyboard.forget, target)
      else
        stage = 5
      end
    elseif stage == 5 then
      if flow == 'pair' then
        if queue('done', keyboard.pair) then
          flow = nil
        end
      elseif target then
        if queue('done', keyboard[flow == 'listen' and 'listen' or 'connect'], target) then
          retry_at = now()
          retries = flow == 'connect' and 1 or 0
          flow = nil
        end
      else
        flow = nil
        confirm = true
      end
    end
  end
  if
    not flow
    and not job
    and not menu
    and not confirm
    and b.state == 0
    and online
    and target
    and age(retry_at) > (retries > 0 and 15000 or 1000)
  then
    if retries > 0 and retries < 3 then
      if queue('done', keyboard.connect, target) then
        retries = retries + 1
      end
    else
      retries = 0
      queue('done', keyboard.listen, target)
    end
    retry_at = now()
  end
end

return {
  init = function()
    assert(keyboard.mode, 'firmware 306025 required')
    brightness(20)
    lights.fill(0)
    lights.present()
    local s = storage.get()
    if s then
      local peer, list = s:match('^TV[23]|([^|]+)|(.*)$')
      if peer then
        if peer ~= '-' and peer:match('^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$') then
          target = peer
        end
        local valid, used = true, {}
        for k in list:gmatch('%d+') do
          k = tonumber(k)
          if k > 10 or used[k] then
            valid = false
          end
          used[k] = true
          keys[#keys + 1] = k
        end
        if not valid or #keys > #labels or table.concat(keys, ',') ~= list then
          keys = {}
        end
      end
    end
    keyboard.status(b)
    errors = b.errors
    if b.connected then
      target = b.peer
      changed()
    end
    begin(target and 'connect' or 'listen')
    screen('TV', 'FOLLOW THE SCREEN')
  end,
  key = function(k, event)
    if event ~= 1 then
      return
    end
    if #keys < #labels then
      for _, v in ipairs(keys) do
        if v == k then
          hint('PICK A DIFFERENT KEY')
          return
        end
      end
      keys[#keys + 1] = k
      notice = nil
      changed()
      return
    end
    local role
    for i, v in ipairs(keys) do
      if v == k then
        role = i
      end
    end
    if not role then
      hint('USE YOUR MENU KEY')
      return
    end
    if confirm then
      if role == 1 then
        begin('pair')
      elseif role == 4 or role == 2 then
        confirm = false
      end
    elseif role == 4 then
      menu = not menu and 1 or nil
      notice = nil
    elseif menu then
      if role == 7 or role == 8 then
        menu = (menu - 1 + (role == 7 and -1 or 1)) % #choices + 1
      elseif role == 1 then
        select()
      elseif role == 2 then
        menu = nil
      end
    elseif not target and not b.connected then
      confirm = true
    else
      press(role)
    end
  end,
  message = function(s)
    if s == 'toggle' then
      s = 'play_pause'
    end
    if s == 'bind' then
      keys = {}
      changed()
    elseif s == 'pair' or s == 'connect' or s == 'listen' then
      begin(s)
    elseif s == 'disconnect' then
      begin('off')
    elseif s == 'status' then
      app.log(b.state .. ' ' .. (b.peer or '-'))
    elseif s == 'menu' then
      menu = not menu and 1 or nil
    else
      local peer = s:match('^target (%x%x:%x%x:%x%x:%x%x:%x%x:%x%x)$')
      if peer then
        target = peer:upper()
        changed()
      else
        for i, name in ipairs(actions) do
          if s == name then
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
        local kind = job.name
        job = nil
        if not ok then
          flow = nil
          hint(kind == 'save' and 'SAVE FAILED RETRY' or 'DISCONNECT OTHER AUDIO')
        elseif kind == 'save' then
          dirty = config() ~= saving
        elseif kind == 'step' then
          stage = stage + 1
          stage_at = now()
        end
      end
    end
    if b.connected then
      if not linked then
        target = b.peer
        changed()
        notice = nil
      end
      retries = 0
      retry_at = now()
    end
    linked = b.connected
    if pending and not job and not b.busy and age(key_at) >= 200 then
      local a = pending
      pending = nil
      press(a)
    end
    connect()
    if dirty and not flow and not job and age(save_at) > 1200 then
      saving = config()
      queue('save', storage.set, saving)
    end
    if b.errors ~= errors then
      errors = b.errors
      hint('LINK FAILED TAP MENU')
    end
    if notice and age(shown) > 3500 then
      notice = nil
    end
    if confirm then
      screen('PAIR', 'RESET PAIRING LEVER YES M BACK', 0xffa030)
    elseif menu then
      screen(choices[menu], 'ARROWS BROWSE LEVER OK M BACK')
    elseif notice then
      screen(icons[notice] or 'INFO', labels[notice] and labels[notice] .. ' SENT' or notice)
    elseif #keys < #labels then
      screen(labels[#keys + 1], 'PRESS KEY FOR ' .. labels[#keys + 1])
    elseif flow then
      screen('WAIT', 'SETTING UP')
    elseif dirty then
      screen('SAVE', 'KEEP POWER ON')
    elseif b.connected then
      screen('TV', 'READY  M MENU', 0x20ff40)
    elseif b.pairing then
      screen(
        'PAIR',
        (120000 - b.pair_remaining_ms) // 20000 % 2 == 0 and 'TV FORGET DITOO THEN PAIR ACCESSORY'
          or 'PICK DITOOPRO AUDIO ACCEPT'
      )
    elseif b.state == 1 then
      screen('LINK', 'ACCEPT ON TV')
    elseif not target then
      screen('TV', 'TAP PLAY TO PAIR YOUR TV')
    else
      screen('WAIT', 'CONNECT ON TV OR TAP MENU')
    end
    local value, total
    if b.pairing then
      value, total = b.pair_remaining_ms, 120000
    elseif #keys < #labels then
      value, total = #keys + 1, #labels + 1
    elseif menu then
      value, total = menu, 5
    end
    view.draw(b.connected, value, total)
  end,
}
