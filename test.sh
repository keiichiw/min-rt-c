#!/bin/bash
make all
for i in ./origin/sld/*.sld
do
    f="${i%.sld}"
    g="${f##*/}"
    echo $f
    ./conv <$f.sld >./test/$g.bin
    ./min-rt <./test/$g.bin >./test/$g.ppm
done
