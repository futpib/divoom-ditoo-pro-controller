-- Stock microphone values are not calibrated sound-pressure measurements.
local ticket
local elapsed=0
return {
 init=function() ticket=assert(microphone.noise(true)) end,
 update=function(dt)
  local ok,err=device.result(ticket)
  if ok==false then error(err) end
  elapsed=elapsed+dt
  if elapsed<250 then return end
  elapsed=0
  local level,age=microphone.level()
  if not level or age>2000 then return end
  display.clear(0)
  display.text(0,0,tostring(level),0x40a0ff)
  local h=math.max(0,math.min(9,math.floor((level-30)/5)))
  display.rect(0,16-h,16,h,0x008020,true)
  display.present()
 end,
}
