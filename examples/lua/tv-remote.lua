-- Requires firmware 306017. Send `connect XX:XX:XX:XX:XX:XX` to choose a TV.
-- Native AVRCP connection; the stock stack may also connect TV audio.
-- Press the desired button once to bind it, then press it to play/pause.
-- `lua send toggle` also tests the control without a physical key press.
local button,ticket,pending,last_press,target,connecting,last_state
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
  app.log('selected TV is not connected');return
 end
 local now=time.millis()
 if last_press and ((now-last_press)&0x7fffffff)<500 then return end
 last_press=now
 local err
 pending=action or next_action
 ticket,err=bluetooth.media(pending)
 if not ticket then app.log(err);return end
 display.clear(0)
 if pending=='pause' then
  display.rect(4,3,3,10,0x40ff60,true)
  display.rect(9,3,3,10,0x40ff60,true)
 else
  for x=0,5 do display.line(4+x,3+x,4+x,13-x,0x40ff60) end
 end
 display.present()
end

return {
 init=function()
  assert(bluetooth,'firmware 306017 required')
  brightness(20)
  show('TV')
  app.log('send connect XX:XX:XX:XX:XX:XX; press a key to bind')
 end,
 key=function(key,event)
  if event~=1 then return end
  if button==nil then
   button=key
   app.log('bound key '..key..'; press again to play/pause')
   show(tostring(key),0x40ff60)
  elseif key==button then control() end
 end,
 message=function(command)
  if command=='toggle' then control()
  elseif command=='play' or command=='pause' then control(command)
  elseif command=='bind' then button=nil;show('KEY');app.log('press a key to bind')
  elseif command:sub(1,8)=='connect ' then
   if ticket or connecting then app.log('connection pending');return end
   target=command:sub(9):upper()
   assert(#target==17 and target:match('^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$'),'invalid TV address')
   ticket=assert(audio.source('bluetooth'));pending='source';show('BT')
  elseif command=='disconnect' then
   if ticket then app.log('remote busy');return end
   ticket=assert(bluetooth.disconnect_media());pending='disconnect';connecting=nil
  elseif command=='status' then
   local b=bluetooth.status()
   app.log('peer='..tostring(b.peer)..' media='..b.media_state..' audio='..b.audio_state..' BLE='..tostring(b.ble_connected))
  else app.log('send connect ADDRESS, toggle, play, pause, bind, status, or disconnect') end
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
    app.log(pending..' submitted; TV response not confirmed')
   end
  end
  local b=bluetooth.status()
  local state=b.media_state..':'..b.audio_state..':'..tostring(b.peer)
  local connected=b.media_connected and b.peer==target
  if state~=last_state or (connecting and connected) then
   last_state=state
   if connected then
    connecting=nil;show(button and tostring(button) or 'KEY',0x40ff60)
    app.log('TV media connected; audio='..b.audio_state..'; BLE='..tostring(b.ble_connected))
   elseif not connecting then show('TV');app.log('no selected TV connection') end
  end
  if connecting and ((time.millis()-connecting)&0x7fffffff)>=20000 then
   connecting=nil;show('ERR',0xff4040)
   app.log('no TV connection after 20s; native attempt may still finish')
  end
 end,
}
