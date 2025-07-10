#!/bin/sh
set -e

# build
make clean >/dev/null
make >/dev/null

# help and version output
./rkcfgtool --help > /dev/null
./rkcfgtool --version > /dev/null

# list all sample cfgs
for f in cfg/*.cfg; do
    ./rkcfgtool "$f" > /dev/null
done

# create new cfg and read it back
./rkcfgtool --create tmp.cfg --add foo bar > /dev/null
./rkcfgtool tmp.cfg > /dev/null
rm -f tmp.cfg

echo "OK"
