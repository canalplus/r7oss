#!/bin/sh

# written by Grzegorz Dabrowski <gdx@poczta.fm>

>/tmp/all.i
#cd /etc/initng
cd ../initfiles
for i in `find . -name '*.i'`
do
  j=${i#./}
  cat $i >> /tmp/all.i
done
cd -

perl ./analyze.pl
dot -Tpng need.dot > need.png
dot -Tpng use.dot > use.png
