#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
python3 make_synthetic_fixtures.py
python3 make_headers.py
${CXX:-g++} ${HOST_GCC_PREFIX:+-B$HOST_GCC_PREFIX} -std=c++17 -Wall -Wextra -Wpedantic -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer test_core.cpp ../../control_core.cpp -o test_core
./test_core
python3 - <<'PY'
import json
from pathlib import Path
items=[json.loads(x) for x in Path('discovery.jsonl').read_text().splitlines()]
assert len(items)==19
controls=[i for i in items if 'command_topic' in i]
assert len(controls)==3
for c in controls:
 assert c['retain'] is False and c['optimistic'] is False
 assert c['state_topic'] and c['availability_mode']=='all' and len(c['availability'])==2
m=next(c for c in controls if c['command_topic'].endswith('/mode'))
assert m['options']==['Compliance Level 1','Compliance Level 2','Compliance Level 3','Pause']
b=next(c for c in controls if c['command_topic'].endswith('/bypass'));assert b['options']==['Auto','Closed','Open']
a=next(c for c in controls if c['command_topic'].endswith('/auto'))
assert a['state_on']=='1' and a['state_off']=='0'
assert a['payload_on']=='0123456789abcdef:ON' and a['payload_off']=='0123456789abcdef:OFF'
assert m['command_template']=='0123456789abcdef:{{ value }}'
for x in items:
 assert x['device']['identifiers']==['pa700li']
 if 'command_topic' not in x:assert x['expire_after']==30
assert not any(x.get('command_topic','').endswith('/boost') or x.get('command_topic','').endswith('/party') for x in items)
print('PASS independent Python JSON/discovery schema assertions (19 entities)')
PY
