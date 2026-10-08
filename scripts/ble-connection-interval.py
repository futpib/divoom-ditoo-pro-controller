#!/usr/bin/env python3
"""Request a BLE interval for an already-connected device; requires raw-HCI access."""
import argparse
import ctypes
import fcntl
import json
import os
import socket
import struct
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('device', help='Explicit connected Bluetooth MAC')
p.add_argument('--adapter', type=int, default=0)
p.add_argument('--milliseconds', type=float, default=7.5)
p.add_argument('--supervision-ms', type=int, default=5000,
               help='Disconnect timeout in ms, in multiples of 10 (100..32000; default 5000)')
a = p.parse_args()
units = round(a.milliseconds / 1.25)
if not 6 <= units <= 3200 or units * 1.25 != a.milliseconds:
    p.error('interval must be 7.5..4000 ms in multiples of 1.25 ms')
if not 100 <= a.supervision_ms <= 32000 or a.supervision_ms % 10:
    p.error('supervision timeout must be 100..32000 ms in multiples of 10 ms')
# With zero latency, supervision must exceed twice the requested interval.
if units * 1.25 * 2 >= a.supervision_ms:
    p.error('supervision timeout must exceed twice the connection interval')
address = bytes.fromhex(a.device.replace(':', ''))[::-1]
if len(address) != 6: p.error('invalid MAC')
b = ctypes.CDLL('libbluetooth.so.3', use_errno=True)
b.hci_open_dev.argtypes = [ctypes.c_int]
b.hci_send_cmd.argtypes = [ctypes.c_int, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_void_p]
fd = b.hci_open_dev(a.adapter)
if fd < 0: raise OSError(ctypes.get_errno(), 'hci_open_dev')
try:
    connections = bytearray(struct.pack('<HH', a.adapter, 32) + bytes(32 * 16))
    fcntl.ioctl(fd, 0x800448d4, connections, True)  # HCIGETCONNLIST, _IOR('H', 212, int)
    count = struct.unpack_from('<H', connections, 2)[0]
    matches = []
    for i in range(min(count, 32)):
        handle, bdaddr, kind, outgoing, state, mode = struct.unpack_from('<H6sBBHI', connections, 4 + i * 16)
        if bdaddr == address and kind == 0x80 and state == 1:
            matches.append(handle)  # Kernel LE_LINK, BT_CONNECTED; exclude pending connects.
    if len(matches) != 1: raise RuntimeError('Expected exactly one live LE connection for this MAC')
    handle = matches[0]
    with socket.fromfd(fd, socket.AF_BLUETOOTH, socket.SOCK_RAW, socket.BTPROTO_HCI) as monitor:
        monitor.setsockopt(0, 2, struct.pack('<IIIH2x', 1 << 4, (1 << 14) | (1 << 15), 1 << 30, 0))
        monitor.settimeout(5)
        data = ctypes.create_string_buffer(struct.pack('<7H', handle, units, units, 0, a.supervision_ms//10, 1, 1))
        if b.hci_send_cmd(fd, 8, 0x13, 14, data): raise OSError(ctypes.get_errno(), 'hci_send_cmd')
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            monitor.settimeout(max(.001, deadline-time.monotonic()))
            event = monitor.recv(260)
            if (len(event) >= 7 and event[:3] == b'\x04\x0f\x04'
                    and event[5:7] == b'\x13\x20' and event[3]):
                raise RuntimeError(f'LE connection update rejected: HCI status 0x{event[3]:02x}')
            if len(event) >= 13 and event[:2] == b'\x04\x3e' and event[3] == 3:
                status, event_handle, interval, latency, supervision = struct.unpack_from('<B4H', event, 4)
                if event_handle != handle: continue
                result = dict(device=a.device, adapter=a.adapter, handle=handle, status=status,
                              interval_ms=interval*1.25, latency=latency, supervision_ms=supervision*10)
                print(json.dumps(result))
                if status or interval != units or latency or supervision*10 != a.supervision_ms:
                    raise RuntimeError('Controller did not apply the requested connection parameters')
                break
        else: raise TimeoutError('No matching LE connection-update event')
finally:
    os.close(fd)
