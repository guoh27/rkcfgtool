#!/bin/sh
set -e

echo "Building..."
make >/dev/null

echo "Checking help and version..."
./rkcfgtool --help | grep -q Usage
./rkcfgtool --version | grep -q rkcfgtool

echo "Validating sample cfgs..."
for f in cfg/*.cfg; do
    ./rkcfgtool "$f" --json | python3 -m json.tool > /dev/null
done

echo "Parsing non-JSON output..."
./rkcfgtool cfg/config1.cfg > list.txt
grep -q "=== Entry list" list.txt
test $(grep -c '^ [0-9]' list.txt) -eq 10
grep '^ 0 ' list.txt | grep -q loader
rm list.txt

echo "Modifying existing cfg..."
cp cfg/config1.cfg t1.cfg
./rkcfgtool t1.cfg --set-path 0 new.bin --set-name 0 bootloader \
  --add xxx yyy.img --del 1 >/dev/null
./rkcfgtool t1.cfg --json > out.json
python3 - out.json <<'EOF'
import json,sys
data=json.load(open(sys.argv[1]))
assert len(data) == 10
assert data[0]['name'] == 'bootloader'
assert data[0]['path'] == 'new.bin'
assert data[-1]['name'] == 'xxx' and data[-1]['path'] == 'yyy.img'
assert [e['index'] for e in data] == list(range(len(data)))
EOF
rm -f t1.cfg out.json

echo "Creating new cfg..."
./rkcfgtool t3.cfg --create --add foo bar.img --add baz qux.bin --del 0 --set-path 0 final.img >/dev/null
./rkcfgtool t3.cfg --json > out.json
python3 - out.json <<'EOF'
import json,sys
data=json.load(open(sys.argv[1]))
assert len(data) == 1
assert data[0]['name'] == 'baz' and data[0]['path'] == 'final.img'
EOF
rm -f t3.cfg out.json

echo "Creating cfg from scratch..."
./rkcfgtool t4.cfg --create --add a a.img --add b b.img --add c c.img --set-name 1 bb >/dev/null
./rkcfgtool t4.cfg --json > out.json
python3 - out.json <<'EOF'
import json,sys
data=json.load(open(sys.argv[1]))
assert len(data) == 3
assert data[1]['name'] == 'bb'
assert [e['index'] for e in data] == list(range(len(data)))
EOF
rm -f t4.cfg out.json

echo "All tests passed"
