-- Firmware 306020. Connect ADDRESS, then press three different keys to bind
-- play/pause, mute and Space. Binding sends nothing. AVRCP remains available.
local keys,ticket,pending,last,state,errors,target={},nil,nil,nil,-1,0,nil
local function show(s)
 display.clear(0);display.text(0,5,s,0x20ff80);display.present()
end
local function tap(action)
 if ticket then return end
 local b=keyboard.status()
 if not b.connected or b.busy then app.log('keyboard disconnected or busy');return end
 local now=time.millis()
 if last and ((now-last)&0x7fffffff)<300 then return end
 last=now
 local err
 if action=='space' then ticket,err=keyboard.tap(44)
 else ticket,err=keyboard.media(action) end
 if not ticket then app.log(err) else pending=action;show(action=='space' and 'SP' or action=='mute' and 'M' or 'P') end
end
return {
 init=function()
  assert(keyboard,'firmware 306020 required');errors=keyboard.status().errors;brightness(20);show('KB')
  app.log('connect ADDRESS; bind play/pause, mute, Space')
 end,
 key=function(key,event)
  if event~=1 then return end
  for i,k in ipairs(keys) do
   if key==k then tap(i==1 and 'play_pause' or i==2 and 'mute' or 'space');return end
  end
  if #keys<3 then keys[#keys+1]=key;show(tostring(#keys));app.log('bound '..#keys..' to '..key) end
 end,
 message=function(s)
  if s=='toggle' or s=='play_pause' then tap('play_pause')
  elseif s=='mute' or s=='space' then tap(s)
  elseif s=='bind' then keys={};show('KEY')
  elseif s:sub(1,8)=='connect ' then
   if ticket then return end
   target=s:sub(9):upper();ticket=assert(audio.source('bluetooth'));pending='source';show('BT')
  elseif s=='disconnect' then
   if ticket then return end
   ticket=assert(keyboard.disconnect());pending='disconnect'
  elseif s=='status' then
   local b=keyboard.status()
   app.log('state='..b.state..' peer='..tostring(b.peer)..' sent='..b.sent..' released='..b.released..' error='..b.error..' bonds='..b.bonds_saved)
  end
 end,
 update=function()
  if ticket then
   local ok,err=device.result(ticket)
   if ok~=nil then
    ticket=nil
    if ok and (pending=='source' or pending=='media_disconnect') then
     local b=bluetooth.status()
     if pending=='source' and b.media_connected and b.peer~=target then
      ticket=assert(bluetooth.disconnect_media());pending='media_disconnect'
     else ticket=assert(keyboard.connect(target));pending='connect' end
    else app.log(ok and pending..' queued' or err) end
   end
  end
  local b=keyboard.status()
  if b.state~=state then state=b.state;show(state==2 and 'KEY' or state==1 and 'BT' or 'KB') end
  if b.errors~=errors then errors=b.errors;show('ERR');app.log('HID error '..b.error) end
 end,
}
