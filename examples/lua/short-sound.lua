-- Start silently, then send `play` with `lua send play`.
-- Preview volume is independent of music volume. Begin quietly.
local mode,level,duration=6,15,300
local ticket,phase,started,timed_out

local function stop()
 ticket=assert(audio.stop())
 phase='stopping'
end

return {
 init=function()
  app.claim(false)
  app.log('send play for a quiet 300 ms sound')
 end,
 message=function(command)
  if command~='play' then app.log('send play');return end
  if phase then app.log('sound pending');return end
  ticket=assert(audio.preview(mode,level))
  started=time.millis()
  phase='starting'
  timed_out=false
 end,
 update=function()
  if ticket then
   local ok,err=device.result(ticket)
   if ok==nil then return end
   assert(ok,err)
   ticket=nil
   if phase=='stopping' then
    phase=nil
    app.log(timed_out and 'playback did not start' or 'stop request completed')
    return
   end
  end
  local now=time.millis()
  local elapsed=started and ((now-started)&0x7fffffff) or 0
  if phase=='starting' then
   if audio.status().sound_playing then started=now;phase='playing'
   elseif elapsed>=2000 then timed_out=true;stop() end
  elseif phase=='playing' and elapsed>=duration then stop() end
 end,
}
