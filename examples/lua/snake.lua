-- Numeric physical-key IDs can be changed for a different keyboard mapping.
-- Host messages "up", "down", "left", "right" use the same game logic.
local body, dx, dy, nextdx, nextdy, food, elapsed, score, lost
local function reset()
  body = {{8, 8}, {7, 8}, {6, 8}}
  dx, dy, nextdx, nextdy, elapsed, score, lost = 1, 0, 1, 0, 0, 0, false
  food = {3, 3}
end
local directions = {up={0,-1}, down={0,1}, left={-1,0}, right={1,0}}
local function steer(name)
  local d = directions[name]
  if d and (d[1] ~= -dx or d[2] ~= -dy) then nextdx, nextdy = d[1], d[2] end
end
local function draw()
  display.clear(0)
  display.pixel(food[1], food[2], 0xff2020)
  for _, p in ipairs(body) do display.pixel(p[1], p[2], lost and 0xff8020 or 0x40ff60) end
  display.present()
end
return {
  init = function() brightness(20); reset(); draw() end,
  key = function(k, event)
    if event ~= 1 then return end
    local names = {[2]='right', [3]='left', [4]='up', [1]='down'}
    if lost then reset() else steer(names[k]) end
  end,
  message = function(s) if s == 'reset' then reset() else steer(s) end end,
  update = function(dt)
    elapsed = elapsed + dt
    if elapsed < 200 or lost then return end
    elapsed = 0; dx, dy = nextdx, nextdy
    local x, y = (body[1][1]+dx)%16, (body[1][2]+dy)%16
    local eating = x == food[1] and y == food[2]
    if not eating then table.remove(body) end
    for _, p in ipairs(body) do if p[1] == x and p[2] == y then lost = true end end
    table.insert(body, 1, {x,y})
    if eating then
      score = score+1; app.log('score ' .. score)
      -- A bounded scan also handles an almost full board without a retry loop.
      for offset = 1,256 do
        local i = (x+y*16+offset*73)%256
        local free = true
        for _, p in ipairs(body) do if p[1]+p[2]*16 == i then free=false; break end end
        if free then food={i%16,i//16}; break end
      end
    end
    draw()
  end,
}
