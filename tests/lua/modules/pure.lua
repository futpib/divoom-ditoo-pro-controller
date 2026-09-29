local m = {}
function m.value()
  return require('pure_leaf').value()
end
function m.nested()
  for i = 1, 1 do
    while false do end
  end
  repeat
    if true then do end end
  until true
  return function() return m.value() end
end
function m.other()
  return 7
end
function m.unused()
  return require('unused')
end
return m
