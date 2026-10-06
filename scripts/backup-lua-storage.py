#!/usr/bin/env python3
"""Read the 128 KiB stock filesystem metadata through the 306015-306029 diagnostic."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('device')
p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--probe-only', action='store_true', help='Read just the first four pages')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)

def query(payload):
    return {'command':'0x37','payload_hex':payload.hex(),'response':'0x37'}

def batch(name, requests, timeout):
    source=a.output/(name+'-requests.json')
    source.write_text(json.dumps(requests,indent=2)+'\n')
    with (a.output/(name+'-replies.jsonl')).open('w') as out, (a.output/(name+'-stderr.log')).open('w') as err:
        subprocess.run([str(a.binary),'--device',a.device,'--transport','ble','raw','run',str(source)],
                       stdout=out,stderr=err,check=True,timeout=timeout)
    rows=[json.loads(s) for s in (a.output/(name+'-replies.jsonl')).read_text().splitlines()]
    assert len(rows)==len(requests), 'Missing replies'
    for row in rows:
        assert row['response']['ack'], 'Negative acknowledgement'
    return [bytes.fromhex(row['response']['data_hex']) for row in rows]

version, = batch('preflight',[query(b'\0')],45)
assert len(version)==5 and version[0]==1, 'Invalid firmware version; no diagnostic sent'
installed=int.from_bytes(version[1:],'little')
assert installed in (306015,306016,306017,306018,306019,306020,306021,306022,306023,306024,306025,306026,306027,306028,306029), 'Unsupported Lua storage diagnostic firmware; no diagnostic sent'
units=list(range(8 if a.probe_only else 1024))
requests=[query(b'\x7fDLUA\x0a'+struct.pack('<HB',unit,1)) for unit in units]
requests.append(query(b'\0'))
rows=batch('backup',requests,600)
assert rows.pop()==version, 'Firmware changed during backup'
contents=bytearray()
snapshots=[]
for unit,data in zip(units,rows):
    assert len(data)==176 and data[:6]==b'DFSP\x01\x00', f'Invalid read reply at unit {unit}: {data[:16].hex()}'
    assert int.from_bytes(data[6:8],'little')==unit, 'Wrong read unit'
    assert int.from_bytes(data[8:12],'little')==0, 'Flash driver read failed'
    assert int.from_bytes(data[12:16],'little')==128, 'Truncated data'
    snapshots.append(data[16:48])
    contents.extend(data[48:])
assert all(s==snapshots[0] for s in snapshots), 'Filesystem metadata changed during backup'
context=snapshots[0]
summary={
    'firmware':installed,'read_only':True,'physical_start':0x900000,
    'bytes':len(contents),'sha256':hashlib.sha256(contents).hexdigest(),
    'context_hex':context.hex(),
    'driver_statuses':sorted(set(int.from_bytes(d[8:12],'little') for d in rows)),
    'used_blocks':int.from_bytes(context[:2],'little'),
    'block_count':int.from_bytes(context[28:32],'little'),
    'index_start_page':int.from_bytes(context[20:22],'little'),
    'index_pages':int.from_bytes(context[22:24],'little'),
}
entries=[]
if not a.probe_only:
    start=0x10000+summary['index_start_page']*256
    count=summary['index_pages']
    assert start+count*256<=len(contents), 'Active index outside metadata region'
    for page in range(count):
        for offset in range(start+page*256+8,start+(page+1)*256,8):
            block,model,file_id=struct.unpack_from('<HHI',contents,offset)
            if block==0xffff: continue
            assert block<summary['block_count'], f'Invalid block index {block}'
            entries.append({'block':block,'model':model,'id':file_id})
    summary['files']=entries
    summary['models']={str(k):v for k,v in sorted(Counter(e['model'] for e in entries).items())}
    summary['lua_model_0x4c55_unused']=not any(e['model']==0x4c55 for e in entries)
(a.output/'metadata.bin').write_bytes(contents)
(a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k!='files'},indent=2))
