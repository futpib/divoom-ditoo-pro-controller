-- Requires firmware 306018. Send `connect XX:XX:XX:XX:XX:XX` to choose a TV.
-- Native AVRCP connection; the stock stack may also connect TV audio.
-- First key binds play/pause; first different key binds mute/unmute toggle.
-- Binding sends nothing; press a bound key again to control the TV.
-- `lua send toggle` also tests the control without a physical key press.
local button,mute_button,ticket,pending,last_press,target,connecting,last_state
-- AVRCP without A2DP does not keep audio.status().playing in sync with the TV.
-- Start with pause for a playing video; explicit play/pause resynchronizes us.
local next_action='pause'

local function show(message,color)
 display.clear(0)
 display.text(0,5,message,color or 0x20a0ff)
 display.present()
end

local function control(action)
 if ticket then app.log('remote busy');return end
 local bt=bluetooth.status()
 if not target or not bt.media_connected or bt.peer~=target then
  app.log('TV disconnected');return
 end
 local now=time.millis()
 if last_press and ((now-last_press)&0x7fffffff)<500 then return end
 last_press=now
 local err
 pending=action or next_action
 if pending=='mute' then ticket,err=bluetooth.mute()
 else ticket,err=bluetooth.media(pending) end
 if not ticket then app.log(err);return end
 display.clear(0)
 if pending=='pause' then
  display.rect(4,3,3,10,0x40ff60,true)
  display.rect(9,3,3,10,0x40ff60,true)
 elseif pending=='play' then
  for x=0,5 do display.line(4+x,3+x,4+x,13-x,0x40ff60) end
 else
  display.text(5,5,'M',0x40ff60)
 end
 display.present()
end

return {
 init=function()
  assert(bluetooth and bluetooth.mute,'firmware 306018 required')
  brightness(20)
  show('TV')
  app.log('connect ADDRESS; bind play/pause then mute')
 end,
 key=function(key,event)
  if event~=1 then return end
  if button==nil then
   button=key
   app.log('play/pause key '..key)
   show(tostring(key),0x40ff60)
  elseif key==button then control()
  elseif mute_button==nil then
   mute_button=key
   app.log('mute toggle key '..key)
   show('M',0x40ff60)
  elseif key==mute_button then control('mute') end
 end,
 message=function(command)
  if command=='toggle' then control()
  elseif command=='play' or command=='pause' or command=='mute' then control(command)
  elseif command=='bind' then button=nil;mute_button=nil;show('KEY');app.log('bind play/pause then mute toggle')
  elseif command:sub(1,8)=='connect ' then
   if ticket or connecting then app.log('connection pending');return end
   target=command:sub(9):upper()
   ticket=assert(audio.source('bluetooth'));pending='source';show('BT')
  elseif command=='disconnect' then
   if ticket then app.log('remote busy');return end
   ticket=assert(bluetooth.disconnect_media());pending='disconnect';connecting=nil
  elseif command=='status' then
   local b=bluetooth.status()
   app.log('peer='..tostring(b.peer)..' media='..b.media_state..' audio='..b.audio_state..' BLE='..tostring(b.ble_connected))
  else app.log('unknown command') end
 end,
 update=function()
  if ticket then
   local ok,err=device.result(ticket)
   if ok==nil then return end
   ticket=nil
   if not ok then connecting=nil;app.log(err);show('ERR',0xff4040);return end
   if pending=='source' then
    ticket=assert(bluetooth.connect_media(target));pending='connect'
    connecting=time.millis();return
   else
    if pending=='play' then next_action='pause'
    elseif pending=='pause' then next_action='play' end
    app.log(pending..' queued; TV unconfirmed')
   end
  end
  local b=bluetooth.status()
  local connected=b.media_connected and b.peer==target
  if connected~=last_state or (connecting and connected) then
   last_state=connected
   if connected then
    connecting=nil;show(button and tostring(button) or 'KEY',0x40ff60)
    app.log('TV media connected')
   elseif not connecting then show('TV');app.log('TV disconnected') end
  end
  if connecting and ((time.millis()-connecting)&0x7fffffff)>=20000 then
   connecting=nil;show('ERR',0xff4040)
   app.log('20s timeout; connection may still finish')
  end
 end,
}
