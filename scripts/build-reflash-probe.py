#!/usr/bin/env python3
"""Build the narrowly scoped 306008 reflash experiment from pinned stock bytes."""
import binascii
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
STOCK_SHA = 'fc16341c005b11d0ac476917dc2fd98c9bc7481b92a183e64bfe209801566544'
CODE = 0x60f


def crc_code(code, end):
    crc = 0
    for start, stop in [(0, 0xa4), (0xa8, 0xbc), (0xc0, 0xcc), (0xd4, 0xe4), (0xec, end)]:
        crc = binascii.crc_hqx(code[start:stop], crc)
    return crc


def build(stock):
    if hashlib.sha256(stock).hexdigest() != STOCK_SHA:
        raise ValueError('Input must be exact archived stock 306007 firmware')
    patched = bytearray(stock)
    patches = [(0x47924, bytes.fromhex('4404ab57'), bytes.fromhex('4404ab58')),
               (0x4b550, bytes.fromhex('c816'), bytes.fromhex('d516'))]
    for offset, before, after in patches:
        assert patched[CODE+offset:CODE+offset+len(before)] == before
        patched[CODE+offset:CODE+offset+len(before)] = after
    # The application header word is compared for inequality by the bootloader.
    # Its original vendor derivation is unknown; assign a content-derived marker.
    # This is NOT claimed to reproduce the vendor's application fast-CRC scheme.
    app = patched[CODE+0x10000:-4]
    marker = crc_code(app, len(app))
    assert marker != struct.unpack_from('<I', stock, CODE+0x100cc)[0]
    struct.pack_into('<I', patched, CODE+0x100cc, marker)
    code = patched[CODE:-4]
    struct.pack_into('<I', patched, CODE+0xcc, crc_code(code, len(code)))
    struct.pack_into('<I', patched, len(patched)-4, binascii.crc_hqx(patched[:-4], 0))
    code = patched[CODE:-4]
    assert crc_code(code, 0x9e80) == struct.unpack_from('<I', code, 0xbc)[0] == 0x5f08
    assert crc_code(code, len(code)) == struct.unpack_from('<I', code, 0xcc)[0]
    assert binascii.crc_hqx(patched[:-4], 0) == struct.unpack_from('<I', patched, len(patched)-4)[0]
    allowed = set()
    for offset, size in [(CODE+0x47924,4),(CODE+0x4b550,2),(CODE+0x100cc,4),(CODE+0xcc,4),(len(stock)-4,4)]:
        allowed.update(range(offset,offset+size))
    changes = [{'file_offset':i,'code_offset':i-CODE,'before':a,'after':b}
               for i,(a,b) in enumerate(zip(stock,patched)) if a!=b]
    assert len(stock)==len(patched)
    assert all(x['file_offset'] in allowed for x in changes)
    return bytes(patched), {'stock_sha256':STOCK_SHA,'patched_sha256':hashlib.sha256(patched).hexdigest(),
        'version':306008,'bytes':len(patched),'application_change_marker':marker,
        'application_marker_vendor_algorithm_known':False,'changes':changes,
        'package_crc16':binascii.crc_hqx(patched[:-4],0),'code_crc16':crc_code(code,len(code)),
        'bootloader_crc16':crc_code(code,0x9e80)}


if __name__ == '__main__':
    image, report = build((ROOT/'firmware/306007.MVA').read_bytes())
    (ROOT/'firmware/306008-reflash-probe.MVA').write_bytes(image)
    (ROOT/'firmware/306008-reflash-probe.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
