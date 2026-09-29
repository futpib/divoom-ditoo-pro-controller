local a = require('modules.counter')
local b = require('./modules/counter.lua')
assert(a == b and a.count == 1)
if false then require('modules.unused') end
assert(require('modules.false') == false and require('modules.false') == false)
assert(bundle_false == 1)
assert(require('modules.function')() == 42)
assert('value=' .. require('modules.function')() == 'value=42')
assert(require('modules.pure').nested()() == 42)
assert((1 .. .5) == '10.5')
assert(0x1e - 2 == 28)
assert(3 - -- preserve two minus tokens
 - 2 == 5)
local t = {x=3}
assert(t[ [=[x]=] ] == 3)
return 'Bundle execution passed'
