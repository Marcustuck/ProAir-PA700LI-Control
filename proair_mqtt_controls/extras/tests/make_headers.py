# All fixtures here are generated synthetic data, never private captures.
import json,struct
from pathlib import Path
D=Path(__file__).parent
# Independently read the known status/value layouts with Python struct.unpack.
layout={150:[('FAN_SUPPLY',147,'B',0),('FAN_EXTRACT',149,'B',0),('VOLT_SUPPLY',27,'f',192),('VOLT_EXTRACT',32,'f',192)],200:[('MODE_B',109,'B',1)],229:[('AUTO_A',10,'B',0),('BOOST',16,'B',0)],250:[('TEMP_SUPPLY',30,'f',0),('TEMP_EXTRACT',40,'f',0),('TEMP_FRESH',55,'f',0),('TEMP_EXHAUST',60,'f',0)],550:[('HUMIDITY',73,'f',0),('CO2',78,'f',0),('BOOST_MIN',117,'H',0)],800:[('BYPASS',35,'I',1),('BYPASS_ACTIVE',45,'B',0),('BYPASS_POSITION',87,'f',0)],900:[('FILTER_DEPLETION',29,'f',0)],1100:[('MODE_A',46,'I',1),('AUTO_B',97,'B',0),('PARTY',61,'I',1)]}
arr=lambda b:'{'+','.join(hex(x) for x in b)+'}'
s='#pragma once\n#include <vector>\nstruct Expected{proair::Field field;double value;};\nstruct TelemetryFixture{std::vector<uint8_t> bytes;std::vector<Expected> expected;};\nstatic const std::vector<TelemetryFixture> telemetryFixtures={\n'
for f in json.loads((D/'synthetic_telemetry_fixtures.json').read_text())['cases']:
 b=bytes.fromhex(f['hex']);p=b[11:-3];exp=[]
 for name,off,fmt,status in layout[f['block']]:
  assert p[off]==status
  x=struct.unpack_from('<'+fmt,p,off+1)[0]; exp.append('{proair::'+name+','+repr(float(x))+'}')
 s+='{'+arr(b)+',{'+','.join(exp)+'}},\n'
s+='};\nstruct ControlFixture{uint16_t param;std::vector<uint8_t> value,bytes;};\nstatic const std::vector<ControlFixture> controlFixtures={\n'
for k,h in json.loads((D/'synthetic_control_fixtures.json').read_text())['cases'].items():
 p,v=k.split(':');s+='{'+p+','+arr(bytes.fromhex(v))+','+arr(bytes.fromhex(h))+'},\n'
s+='};\n';(D/'fixture_data.h').write_text(s)
