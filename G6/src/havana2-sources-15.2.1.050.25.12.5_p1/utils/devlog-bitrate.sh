#!/bin/sh
 parseNplot.pl \
     --cmd "devdump $1 2>&1 | grep -e 'open\|write'" \
     --timestamp '\[\s*(\S+)\s+=>$1*1000000' \
     "(write\(\s+\d+)\s*,\s*(\d+)#value#among .*#GRAPH Bitrates and write sizes"\
     '(write\(\s+\d+)\s*,\s*(\d+)#value#among .*#CALC my $key = $index;my $diff = ($tsus - $SCRATCH{LAST_TS_US}{$key});  if (!defined $SCRATCH{LAST_TS_US}{$key}) { $SCRATCH{LAST_TS_US}{$key} = $tsus;undef;} else {my $new_value = 1000000.0*%v/$diff/1024/8;$SCRATCH{LAST_TS_US}{$key}=$tsus;$new_value;}#unit kB per sec#SLIDING 100ms#y2#AS Write bitrate#GRAPH Bitrates and write sizes' "(write\(\s+\d+)\s*,\s*#among .*#graph Write scheduling"\
     --compact \
     --live $2 $3 $4 $5 $6 $7 $8 $9
