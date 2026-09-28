-- Test fixture: requires unused power slots 7 and 8; restore both after reboot.
local ticket,phase,off,on,ready,off_delay
return {
 init=function()
  local c=time.calendar()
  local minutes=c.hour*60+c.min+1
  if c.sec>45 then minutes=minutes+1 end
  off=minutes;on=minutes+1;phase=0
  off_delay=time.millis()+((minutes-c.hour*60-c.min)*60-c.sec)*1000
  ticket=assert(power.set_schedule(8,{enabled=true,action='on',hour=math.floor(on/60)%24,min=on%60,days=0,color=0x101010}))
 end,
 update=function()
  if ticket then
   local ok,e=device.result(ticket)
   if ok==nil then return end
   assert(ok,e);ticket=nil
   if phase==0 then phase=1;ready=time.millis()+1100
   else phase=3;app.log('armed:'..math.floor((off_delay-time.millis())/1000)) end
  elseif phase==1 and time.millis()>ready then
   ticket=assert(power.set_schedule(7,{enabled=true,action='off',hour=math.floor(off/60)%24,min=off%60,days=0}))
   phase=2
  elseif phase==3 then app.log('armed:'..math.floor((off_delay-time.millis())/1000))
  end
 end
}
