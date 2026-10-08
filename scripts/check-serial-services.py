#!/usr/bin/env python3
"""Probe remote-mode SDP and audio rejection while a real serial session stays open.

Requires the Ditoo Classic address as the only argument. No pairing, profile
changes or audio negotiation are performed. Prints a compact JSON report.
"""
import errno,json,socket,struct,sys
mac=sys.argv[1]
def element(data,pos=0):
 h=data[pos];pos+=1;kind=h>>3;idx=h&7
 if idx<5:n=1<<idx
 else:
  w=1<<(idx-5);n=int.from_bytes(data[pos:pos+w],'big');pos+=w
 end=pos+n
 assert end<=len(data),(h,pos,n,len(data))
 if kind in [6,7]:
  result=[]
  while pos<end:
   v,pos=element(data,pos);result.append(v)
 elif kind in [1,2,3,5]:result=int.from_bytes(data[pos:end],'big')
 else:result=data[pos:end].hex()
 return result,end
with socket.socket(socket.AF_BLUETOOTH,socket.SOCK_SEQPACKET,socket.BTPROTO_L2CAP) as s:
 s.settimeout(8);s.connect((mac,1));continuation=b'\0';raw=b'';transaction=1
 while True:
  body=bytes.fromhex('3503190100ffff35050a0000ffff')+continuation
  s.send(struct.pack('>BHH',6,transaction,len(body))+body)
  p=s.recv(65535);assert p[0]==7 and int.from_bytes(p[1:3],'big')==transaction,p.hex()
  n=int.from_bytes(p[5:7],'big');raw+=p[7:7+n];continuation=p[7+n:];transaction+=1
  if continuation==b'\0':break
records,end=element(raw);assert end==len(raw)
report={'address':mac,'records':[{str(r[i]):r[i+1] for i in range(0,len(r),2)} for r in records]}
classes=[u for r in report['records'] for u in r.get('1',[])]
assert 0x1101 in classes and not any(u in [0x1108,0x1112,0x111e,0x111f,0x1131] or 0x110a<=u<=0x110f for u in classes),classes
spp=next(r for r in report['records'] if 0x1101 in r.get('1',[]))
server=next(p[1] for p in spp['4'] if p[0]==3)
assert 1<=server<=4
report['spp_server']=server

def version(stream):
 stream.sendall(bytes.fromhex('01040037003b0002'))
 data=b''
 while True:
  while len(data)>=3:
   size=int.from_bytes(data[1:3],'little')+4
   assert data[0]==1 and 9<=size<=65539
   if len(data)<size:break
   frame,data=data[:size],data[size:]
   assert frame[-1]==2 and sum(frame[1:-3])&65535==int.from_bytes(frame[-3:-1],'little')
   if frame[3:7]==bytes.fromhex('04375501'):
    return int.from_bytes(frame[7:-3],'little')
  chunk=stream.recv(4096)
  assert chunk,'Serial session closed'
  data+=chunk

report['rejections']=[]
with socket.socket(socket.AF_BLUETOOTH,socket.SOCK_STREAM,socket.BTPROTO_RFCOMM) as serial:
 serial.settimeout(8);serial.connect((mac,server));report['version_before']=version(serial)
 for protocol,kind,ports in [(socket.BTPROTO_RFCOMM,socket.SOCK_STREAM,[n for n in range(1,5) if n!=server]),
                             (socket.BTPROTO_L2CAP,socket.SOCK_SEQPACKET,[0x17,0x19,0x1b])]:
  for port in ports:
   with socket.socket(socket.AF_BLUETOOTH,kind,protocol) as probe:
    probe.settimeout(5)
    try:probe.connect((mac,port))
    except OSError as e:
     assert e.errno in [errno.ECONNREFUSED,errno.ECONNRESET],(protocol,port,e)
     report['rejections'].append(dict(protocol='rfcomm' if protocol==socket.BTPROTO_RFCOMM else 'l2cap',port=port,errno=e.errno))
    else:raise AssertionError(('Audio path accepted',protocol,port))
 report['version_after']=version(serial)
 assert report['version_after']==report['version_before']
print(json.dumps(report,indent=2),flush=True)
