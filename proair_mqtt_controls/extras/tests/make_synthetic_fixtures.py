"""Generate fictional telemetry and wire frames using non-device test credentials.

These fixtures are NOT recordings, anonymized recordings, or proof of hardware TX.
The known field offsets/control semantics are exercised with invented values.
"""
from pathlib import Path
import binascii,json,struct
D=Path(__file__).parent
LOGIN=b'TESTUSER'  # Deliberately synthetic; never a supplied installation login.
PIN=b'DEMO'        # Deliberately synthetic; never a supplied installation PIN.
def frame(dst,src,func,data):
    body=struct.pack('<HHB',dst,src,func)+data
    covered=struct.pack('<H',len(body))+body
    return b'\x68'+covered+struct.pack('>H',binascii.crc_hqx(covered,0))+b'\x16'
def write(parameter,value):
    return frame(1,0x64,0x29,LOGIN+b'\0'+PIN+b'\0'+b'\1'+struct.pack('<H',parameter)+value)
controls={}
for parameter,values,width in [(225,[8,16,32],1),(241,[0,1],1),(816,[0x00100109,0x00100121,0x00100111],4),(1123,[3,4,5,6],4),(1126,[0],4)]:
    for value in values:
        raw=value.to_bytes(width,'little');controls[f'{parameter}:{raw.hex()}']=write(parameter,raw).hex()
(D/'synthetic_control_fixtures.json').write_text(json.dumps({'fixture_type':'synthetic','description':'Fictional test credentials and recomputed CRCs; NOT device recordings. Documentation-only button/Party encodings are not MQTT commands.','synthetic_credentials':{'login':LOGIN.decode(),'pin':PIN.decode()},'cases':controls},indent=2)+'\n')
SIZES={150:155,200:138,229:20,250:148,550:144,800:153,900:153,1100:146}
telemetry=[]
def response(block,fields):
    data=bytearray(SIZES[block])
    for offset,fmt,value,status in fields:
        data[offset]=status;struct.pack_into('<'+fmt,data,offset+1,value)
    # Exercise internal marker bytes in deliberately unused synthetic space.
    if len(data)>20:data[0:2]=b'\x16\x68'
    payload=bytes([10 if block==229 else 50])+struct.pack('<H',block)+data
    telemetry.append({'block':block,'count':payload[0],'hex':frame(0xFFFF,1,0xC0,payload).hex()})
response(150,[(147,'B',70,0),(149,'B',50,0),(27,'f',7.25,192),(32,'f',5.25,192)])
for mode in (3,4,5,6):response(200,[(109,'B',mode,1)])
for auto,boost in [(0,0),(1,0),(0,2)]:response(229,[(10,'B',auto,0),(16,'B',boost,0)])
response(250,[(30,'f',18.0,0),(40,'f',21.0,0),(55,'f',12.0,0),(60,'f',11.0,0)])
for remaining in (12,65535):response(550,[(73,'f',50.0,0),(78,'f',600.0,0),(117,'H',remaining,0)])
for choice in (0x00100121,0x00100109,0x00100111):
    opened=choice==0x00100111
    response(800,[(35,'I',choice,1),(45,'B',int(opened),0),(87,'f',100.0 if opened else 0.0,0)])
response(900,[(29,'f',25.0,0)])
for mode in (3,4,5,6):response(1100,[(46,'I',mode,1),(97,'B',0,0),(61,'I',0,1)])
assert len(telemetry)==19
(D/'synthetic_telemetry_fixtures.json').write_text(json.dumps({'fixture_type':'synthetic','description':'Generated fictional measurements; no private capture bytes, source identities, request/boot IDs or timestamps.','cases':telemetry},indent=2)+'\n')
print('Generated 13 synthetic control and 19 synthetic telemetry fixtures; no recording input.')
