-- Send record / stop / play / delete / status with `lua send`.
-- The native recorder replaces its single memo and stops after 60 seconds.
local ticket
return {
 init=function() app.log('send record, stop, play, delete or status') end,
 update=function()
  if not ticket then return end
  local ok,err=device.result(ticket)
  if ok==nil then return end
  ticket=nil
  app.log(ok and 'done' or err)
 end,
 message=function(command)
  if ticket then app.log('operation pending');return end
  local err
  if command=='record' then ticket,err=microphone.record()
  elseif command=='stop' then ticket,err=microphone.stop()
  elseif command=='play' then ticket,err=audio.memo_play()
  elseif command=='delete' then ticket,err=audio.memo_delete()
  elseif command=='status' then
   local s=audio.status()
   app.log(table.concat({tostring(s.recording),s.recorded_bytes,device.stats().free_heap},','))
   return
  else app.log('unknown command');return end
  app.log(ticket and 'pending' or err)
 end,
}
