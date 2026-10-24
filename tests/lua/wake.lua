local wake = require('../../lua/wake')
assert(
  wake.xiaomi_app('0C:CD:B4:D0:0C:26')
    == string.char(2, 1, 6, 17, 7, 0, 1, 2, 1, 5, 3, 255, 0, 1, 38, 12, 208, 180, 205, 12, 0)
)
assert(
  wake.xiaomi_app('a0:00:02:03:04:05')
    == string.char(2, 1, 6, 17, 7, 0, 1, 2, 1, 5, 3, 255, 0, 1, 5, 4, 3, 2, 0, 160, 0)
)
for _, address in ipairs({
  false,
  42,
  {},
  '',
  '0C-CD-B4-D0-0C-26',
  'C:CD:B4:D0:0C:26',
  '0G:CD:B4:D0:0C:26',
  '0C:CD:B4:D0:0C:26 ',
  '0C:CD:B4:D0:0C:26:00',
}) do
  assert(not pcall(wake.xiaomi_app, address))
end
assert(not pcall(wake.xiaomi_app, nil))
return 'Wake packets passed'
