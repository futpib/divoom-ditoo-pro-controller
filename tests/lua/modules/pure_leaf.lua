local m = {}
function m.value()
  return 42
end
function m.unused()
  error('unused export')
end
return m
