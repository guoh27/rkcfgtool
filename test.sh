#!/bin/sh
set -e

# build
make >/dev/null

# help and version output
./rkcfgtool --help | grep -q Usage
./rkcfgtool --version | grep -q rkcfgtool

# list all sample cfgs
for f in cfg/*.cfg; do
    ./rkcfgtool "$f" --json | python3 -m json.tool > /dev/null
done

# modify existing cfg
cp cfg/config1.cfg t1.cfg
./rkcfgtool t1.cfg --set-path 0 new.bin --add xxx yyy.img --del 1 -o t2.cfg >/dev/null
./rkcfgtool t2.cfg --json > out.json
python3 - out.json <<'EOF'
import json,sys
data=json.load(open(sys.argv[1]))
assert len(data) == 10
assert data[0]['path'] == 'new.bin'
assert data[-1]['name'] == 'xxx' and data[-1]['path'] == 'yyy.img'
assert [e['index'] for e in data] == list(range(len(data)))
EOF
rm -f t1.cfg t2.cfg out.json

# create new cfg and manipulate entries
./rkcfgtool --create t3.cfg --add foo bar.img --add baz qux.bin --del 0 --set-path 0 final.img >/dev/null
./rkcfgtool t3.cfg --json > out.json
python3 - out.json <<'EOF'
import json,sys
data=json.load(open(sys.argv[1]))
assert len(data) == 1
assert data[0]['name'] == 'baz' and data[0]['path'] == 'final.img'
EOF
rm -f t3.cfg out.json

echo "OK"
