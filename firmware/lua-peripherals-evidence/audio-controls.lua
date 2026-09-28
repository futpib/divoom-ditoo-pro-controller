-- Requires no SD card. Restores the original source and volume.
local ticket,index,wait_until,original_volume,original_source,completed,error_text,preview
local steps={
 {function() return audio.source('usb') end,function(ok) assert(ok and audio.status().source==7) end},
 {function() return audio.source('bluetooth') end,function(ok) assert(ok and audio.status().source==0) end},
 {function() return audio.source('sd') end,function(ok,e) assert(ok==false and e=='no SD card') end},
 {function() return audio.repeat_mode('all') end,function(ok,e) assert(ok==false and e=='SD playback unavailable') end},
 {function() return audio.preview(2,15) end,function(ok,e)
   if ok then assert(alarm.status().state==4);preview='started'
   else assert(e=='native mode blocks preview');preview='native-priority refusal' end
  end},
 {function() return audio.stop() end,function(ok) assert(ok and not audio.status().sound_playing) end},
 {function() return audio.source(original_source==7 and 'usb' or 'bluetooth') end,
  function(ok) assert(ok and audio.status().source==original_source) end},
}
return {
 init=function()
  local a=audio.status();assert((a.source==0 or a.source==7) and not a.sd_present)
  original_volume=device.volume();original_source=a.source;index=1
  ticket=assert(steps[index][1]())
 end,
 update=function()
  if not index then return end
  if ticket then
   local ok,e=device.result(ticket)
   if ok==nil then return end
   completed,error_text=ok,e;ticket=nil
   wait_until=time.millis()+(index==5 and 1200 or 300)
   return
  end
  if wait_until and time.millis()<wait_until then return end
  wait_until=nil
  steps[index][2](completed,error_text);index=index+1
  if index>#steps then
   device.volume(original_volume);app.log('sources-stop passed; preview: '..preview);index=nil;return
  end
  ticket=assert(steps[index][1]())
 end,
}
