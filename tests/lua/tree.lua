local m = require('modules.pure')
local other = require('./modules/pure.lua')
do end local unused = require('modules.pure_leaf')
assert(m.value() == 42 and m.nested()() == 42 and other.other() == 7)
return 'Tree shaking passed'
