local ticket,old
local phase=0
local began=0
local rang=false
local deadline=time.millis()+150000
return {
 init=function() ticket=assert(alarm.get(9)); display.clear(0);display.text(0,5,'TEST',0x003030);display.present() end,
 update=function()
  local now=time.millis()
  if ticket then
   local ok,v=device.result(ticket)
   if ok==nil then return end
   assert(ok,v);ticket=nil
   if phase==0 then
    assert(not v.enabled, "test requires disabled alarm slot 9")
    old=v
    local c=time.calendar()
    local minutes=c.hour*60+c.min+1
    if c.sec>50 then minutes=minutes+1 end
    ticket=assert(alarm.set(9,{enabled=true,hour=math.floor(minutes/60)%24,min=minutes%60,days=0,mode=2,trigger=1,volume=15}))
    phase=1
   elseif phase==1 then phase=2;app.log('armed')
   elseif phase==4 then
    assert(not alarm.status().ringing, 'snooze did not dismiss ringing')
    ticket=assert(alarm.cancel());phase=5
   elseif phase==5 then ticket=assert(alarm.set(9,old));phase=6
   elseif phase==6 then phase=7;app.log(rang and 'ring-snooze-cancel-restore passed' or 'timeout restored') end
  elseif phase==2 and alarm.status().ringing then
   rang=true;began=now;phase=3;app.log('ringing')
  elseif phase==3 and now-began>=3000 then ticket=assert(alarm.snooze());phase=4
  elseif now>deadline and phase~=7 then
   if old then ticket=assert(alarm.set(9,old));phase=6 end
   app.log('alarm timed out')
  end
 end
}
