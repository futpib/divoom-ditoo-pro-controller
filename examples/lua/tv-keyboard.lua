-- Firmware 306022. Install once with `lua install`; setup then uses only keys.
local target,keys,pending=nil,{},nil
local b,job,flow,stage,stage_at={},nil,nil,0,0
local dirty,save_at,retries,retry_at,saving=false,0,0,0,nil
local menu,confirm,notice,notice_at=nil,false,nil,0
local title,hint,color,since='TV','STARTING',0x20a0ff,0
local last_draw,last_key,seen_errors,was_connected=0,0,0,false
local choices={'CONNECT','PAIR','KEYS','OFF','BACK'}
local labels={'PLAY','MUTE','SPACE','MENU'}
local function now() return time.millis() end
local function age(t) return (now()-t)&0x7fffffff end
local function screen(t,h,c)
 if title~=t or hint~=h then title,hint,since=t,h,now();app.log(t..': '..h) end
 color=c or 0x20a0ff
end
local function changed() dirty=true;save_at=now() end
local function settings() return 'TV2|'..(target or '-')..'|'..table.concat(keys,',') end
local function inform(s) notice=s;notice_at=now() end
local function queue(name,fn,...)
 local t,err=fn(...)
 if t then job={id=t,name=name};return true end
 if err~='busy' then inform('WAIT THEN RETRY') end
 return false
end
local function begin(kind)
 if job then inform('PLEASE WAIT');return end
 menu=nil;confirm=false;notice=nil;pending=nil;flow=kind;stage=1;retries=0
end
local function press(action)
 keyboard.status(b)
 if not b.connected then inform('OFFLINE TAP MENU');return end
 if age(last_key)<300 then return end
 if job or b.busy then pending=action;return end
 last_key=now()
 if action=='space' then queue('key',keyboard.tap,44) else queue('key',keyboard.media,action) end
 inform(action=='space' and 'SPACE SENT' or action=='mute' and 'MUTE SENT' or 'PLAY PAUSE SENT')
end
local function select()
 local s=choices[menu]
 if s=='PAIR' then confirm=true;menu=nil
 elseif s=='KEYS' then keys={};menu=nil;changed()
 elseif s=='BACK' then menu=nil
 else begin(s=='OFF' and 'off' or 'connect') end
end
local function paint()
 if age(last_draw)<100 then return end
 last_draw=now();display.clear(0)
 display.text((16-#title*4)//2,0,title,color)
 local text=hint;local p=age(since)//130
 if #text>4 then
  p=p%(#text*4+24);local start=p//4
  text=('    '..text..'  '):sub(start+1,start+5)
  display.text(-(p%4),10,text,0xb0b0b0)
 else display.text((16-#text*4)//2,10,text,0xb0b0b0) end
 display.pixel(15,6,b.connected and 0x20ff40 or 0x503010)
 if b.pairing then
  display.rect(0,7,math.max(1,b.pair_remaining_ms*14//120000),1,0x2080ff,true)
 elseif #keys<4 then display.rect(0,7,(#keys+1)*3,1,0x2080ff,true)
 elseif menu then display.rect(0,7,menu*3,1,0x2080ff,true) end
 display.present()
end
local function connect_step()
  if flow and not job then
   if stage==1 then
    if flow=='connect' and b.connected and b.peer==target then flow=nil
    else queue('step',audio.source,'bluetooth') end
   elseif stage==2 then
    if b.state==0 then stage=3
    elseif flow=='connect' and b.state==4 and b.peer==target then stage=3
    else queue('step',keyboard.disconnect) end
   elseif stage==3 then
    if b.state==0 or (flow=='connect' and b.state==4) then
     if flow=='off' then flow=nil;inform('OFFLINE TAP MENU')
     else
      local m=bluetooth.status()
      if m.media_connected then queue('step',bluetooth.disconnect_media)
      else stage=4 end
     end
    end
   elseif stage==4 and age(stage_at)>2000 then
    if flow=='pair' and target then queue('step',keyboard.forget,target) else stage=5 end
   elseif stage==5 then
    if flow=='pair' then
     if queue('done',keyboard.pair) then flow=nil end
    elseif target then
     if queue('done',keyboard[flow=='listen' and 'listen' or 'connect'],target) then
      retry_at=now();retries=flow=='connect' and 1 or 0;flow=nil
     end
    else flow=nil;confirm=true end
   end
  end
  if not flow and not job and not menu and not confirm and not b.connected and b.state==0 and retries>0 and age(retry_at)>15000 then
   if retries<3 then
    if queue('done',keyboard.connect,target) then retries=retries+1;retry_at=now() end
   else retries=0;queue('done',keyboard.listen,target) end
  end
end

return {
 init=function()
  assert(storage,'firmware 306022 required');brightness(20);lights.fill(0);lights.present()
  local s=storage.get()
  if s then
   local peer,list=s:match('^TV2|([^|]+)|(.*)$')
   if peer then
    if peer~='-' and peer:match('^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$') then target=peer end
    local valid,used=true,{}
    for k in list:gmatch('%d+') do
     k=tonumber(k);if k>10 or used[k] then valid=false end
     used[k]=true;keys[#keys+1]=k
    end
    if not valid or #keys>4 or table.concat(keys,',')~=list then keys={} end
   end
  end
  keyboard.status(b);seen_errors=b.errors
  if b.connected then target=b.peer;changed()
  elseif target then begin('connect') end
  screen('TV','FOLLOW THE SCREEN')
 end,
 key=function(k,event)
  if event~=1 then return end
  if #keys<4 then
   for _,v in ipairs(keys) do if v==k then inform('PICK A DIFFERENT KEY');return end end
   keys[#keys+1]=k;notice=nil;changed();return
  end
  local role
  for i,v in ipairs(keys) do if v==k then role=i end end
  if not role then inform('USE YOUR MENU KEY');return end
  if confirm then
   if role==1 then begin('pair') elseif role==2 or role==4 then confirm=false end
  elseif menu then
   if role==4 or role==3 then menu=menu%#choices+1
   elseif role==1 then select() elseif role==2 then menu=nil end
  elseif role==4 then menu=1;notice=nil
  elseif not target and not b.connected then confirm=true
  else press(role==1 and 'play_pause' or role==2 and 'mute' or 'space') end
 end,
 message=function(s)
  if s=='toggle' or s=='play_pause' then press('play_pause')
  elseif s=='mute' or s=='space' then press(s)
  elseif s=='bind' then keys={};changed()
  elseif s=='menu' then menu=1
  elseif s=='pair' then begin('pair')
  elseif s=='disconnect' then begin('off')
  elseif s=='status' then app.log(b.state..' '..(b.peer or '-'))
  else
   local peer=s:match('^target (%x%x:%x%x:%x%x:%x%x:%x%x:%x%x)$')
   if peer then target=peer:upper();changed() end
  end
 end,
 update=function()
  keyboard.status(b)
  if job then
   local ok,err=device.result(job.id)
   if ok~=nil then
    local kind=job.name;job=nil
    if not ok then flow=nil;inform(kind=='save' and 'SAVE FAILED RETRY' or 'DISCONNECT OTHER AUDIO')
    elseif kind=='save' then dirty=settings()~=saving
    elseif kind=='step' then stage=stage+1;stage_at=now() end
   end
  end
  if b.connected then
   if not was_connected then target=b.peer;changed();notice=nil end
   retries=0
  end
  was_connected=b.connected
  if pending and not job and not b.busy then local a=pending;pending=nil;press(a) end
  connect_step()
  if dirty and not flow and not job and age(save_at)>1200 then
   saving=settings();queue('save',storage.set,saving)
  end
  if b.errors~=seen_errors then seen_errors=b.errors;inform('LINK FAILED TAP MENU') end
  if notice and age(notice_at)>3500 then notice=nil end
  if confirm then screen('PAIR','RESET PAIRING PLAY YES MUTE NO',0xffa030)
  elseif menu then
   local t=choices[menu];screen(t=='CONNECT' and 'LINK' or t=='BACK' and 'BACK' or t,'MENU NEXT PLAY OK MUTE BACK')
  elseif notice then screen('INFO',notice)
  elseif #keys<4 then screen(labels[#keys+1]=='SPACE' and 'SPC' or labels[#keys+1],'PRESS KEY FOR '..labels[#keys+1])
  elseif flow then screen('WAIT','SETTING UP')
  elseif dirty then screen('SAVE','KEEP POWER ON')
  elseif b.connected then screen('TV','READY',0x20ff40)
  elseif b.pairing then screen('PAIR',(120000-b.pair_remaining_ms)//20000%2==0 and
   'TV FORGET DITOO THEN PAIR ACCESSORY' or 'PICK DITOOPRO AUDIO ACCEPT')
  elseif b.state==1 then screen('LINK','ACCEPT ON TV')
  elseif not target then screen('TV','TAP PLAY TO PAIR YOUR TV')
  else screen('WAIT','CONNECT ON TV OR TAP MENU') end
  paint()
 end,
}
