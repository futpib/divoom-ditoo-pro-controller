-- Firmware 306021. Set target below for autonomous connection, or send connect
-- ADDRESS. Press three different keys to bind
-- play/pause, mute and Space. Binding sends nothing. AVRCP remains available.
local target=nil -- Set your TV address here, e.g. '0C:CD:B4:D0:0C:26'.
local keys,ticket,pending,last,state,errors={},nil,nil,nil,-1,0
local operation,automatic,retries,retry_since='connect',false,0,0
local status={}
local function show(s)
 display.clear(0);display.text(0,5,s,0x20ff80);display.present()
end
local function tap(action)
 if ticket then return end
 local b=keyboard.status(status)
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
  assert(keyboard.pair,'firmware 306021 required');local b=keyboard.status(status)
  errors=b.errors;brightness(20);show('KB')
  if b.connected and (not target or b.peer==target) then target=b.peer;automatic=true;show('KEY')
  elseif target then automatic=true;ticket=assert(audio.source('bluetooth'));pending='source' end
  app.log('connect/pair/listen ADDRESS; bind play/pause, mute, Space')
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
  elseif s:sub(1,8)=='connect ' or s:sub(1,7)=='listen ' or s:sub(1,5)=='pair ' then
   if ticket then return end
   operation,target=s:match('^(%a+) (.+)$');target=target:upper()
   automatic=operation=='connect';retries=0
   ticket=assert(audio.source('bluetooth'));pending='source';show('BT')
  elseif s=='disconnect' then
   if ticket then return end
   automatic=false;ticket=assert(keyboard.disconnect());pending='disconnect'
  elseif s=='forget' and target and not ticket then
   automatic=false;ticket=assert(keyboard.forget(target));pending='forget'
  elseif s=='bonds' then app.log(table.concat(keyboard.bonds(),', '))
  elseif s=='status' then
   local b=keyboard.status(status)
   app.log(table.concat({b.state,b.peer or '-',b.error,b.sent,b.released,b.bonds_saved},' '))

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
     else ticket=assert(keyboard[operation](target));pending=operation end
    else
     app.log(ok and pending..' queued' or err)
     if pending=='connect' then retry_since=time.millis() end
    end
   end
  end
  local b=keyboard.status(status)
  if b.state~=state then state=b.state;show(state==2 and 'KEY' or state==1 and 'BT' or state==4 and 'PAIR' or 'KB') end
  if automatic and target and not ticket and b.state==0 and ((time.millis()-retry_since)&0x7fffffff)>=15000 then
   if retries<3 then
    retries=retries+1;retry_since=time.millis();ticket=assert(keyboard.connect(target));pending='connect'
   else automatic=false;ticket=assert(keyboard.listen(target));pending='listen' end
  end
  if b.errors~=errors then errors=b.errors;show('BT?');app.log('HID error '..b.error) end
 end,
}
