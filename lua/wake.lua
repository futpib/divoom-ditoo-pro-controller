-- Candidate wake formats, not guarantees that a particular TV accepts them.
local wake = {}
function wake.mediatek(address)
  assert(type(address) == 'string' and #address == 17, 'Bluetooth address required')
  local octets = {}
  for octet in address:gmatch('%x%x') do
    octets[#octets + 1] = string.char(tonumber(octet, 16))
  end
  assert(#octets == 6 and address:match('^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$'), 'Invalid address')
  -- MediaTek's legacy manufacturer filter: company, reserved, target BD_ADDR,
  -- four masked bytes, then CRKTM. HCI supplies the target address least byte first.
  return string.char(2, 1, 6, 19, 255, 70, 0, 0)
    .. octets[6]
    .. octets[5]
    .. octets[4]
    .. octets[3]
    .. octets[2]
    .. octets[1]
    .. string.char(0, 0, 0, 0)
    .. 'CRKTM'
end
function wake.xiaomi_rc(address)
  assert(type(address) == 'string' and #address == 17, 'Bluetooth address required')
  local a, b, c = address:match('^%x%x:%x%x:%x%x:(%x%x):(%x%x):(%x%x)$')
  assert(a, 'Invalid address')
  -- Captured from Xiaomi RC during a successful wake. The final vendor field
  -- matches the low three target-address bytes; source-address checks are unknown.
  return string.char(2, 1, 5, 3, 255, 0, 1, 6, 8)
    .. 'MI RC'
    .. string.char(
      3,
      3,
      18,
      24,
      4,
      13,
      4,
      5,
      0,
      2,
      10,
      0,
      4,
      254,
      tonumber(c, 16),
      tonumber(b, 16),
      tonumber(a, 16)
    )
end
return wake
