#!/usr/bin/env python3
"""Back up the entire native configuration journal through the bounded 306022–306028 diagnostic.

Read-only; output can contain Bluetooth link keys and is made private.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--binary',type=Path,default=ROOT/'target/release/divoom-ditoo-pro-controller')
a=p.parse_args()
os.umask(0o077)
a.output.mkdir(parents=True,exist_ok=False)
base=[str(a.binary),'--transport','usb','raw','run']
def batch(name,requests):
 source=a.output/(name+'-requests.json');source.write_text(json.dumps(requests))
 with (a.output/(name+'-replies.jsonl')).open('w') as out:
  subprocess.run(base+[str(source)],stdout=out,check=True,timeout=240)
 return [bytes.fromhex(json.loads(s)['response']['data_hex']) for s in (a.output/(name+'-replies.jsonl')).read_text().splitlines()]
def query(payload):return {'command':'0x37','payload_hex':payload.hex(),'response':'0x37'}
version,=batch('version',[query(b'\0')]);assert version in [b'\1'+v.to_bytes(4,'little') for v in range(306022,306029)]
requests=[query(b'\x7fDLUA\x0c'+struct.pack('<HB',i,1)) for i in range(2560)]
rows=batch('backup',requests);assert len(rows)==2560
contents=bytearray();contexts=[]
for i,r in enumerate(rows):
 assert len(r)==176 and r[:6]==b'DCFG\1\0' and int.from_bytes(r[6:8],'little')==i
 assert r[8:12]==b'\0'*4 and int.from_bytes(r[12:16],'little')==128
 contexts.append(r[16:36]);contents.extend(r[48:])
assert all(c==contexts[0] for c in contexts),'Native journal changed while reading; repeat the backup'
(a.output/'config.bin').write_bytes(contents)
summary={'bytes':len(contents),'sha256':hashlib.sha256(contents).hexdigest(),'context_hex':contexts[0].hex(),
 'base_page':int.from_bytes(contexts[0][12:16],'little'),'read_only':True}
(a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary))
