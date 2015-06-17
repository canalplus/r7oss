#! /bin/sh

TYPES="KEY BTN"
file=${1:-/usr/include/linux/input.h}

for type in $TYPES; do
	grep "^#define ${type}_" < $file|sort|sed -n --expression="s/^#define \([^ 	]*\)[ 	][ 	]*\([0-9][0-9a-fA-FxX]*\).*/{\"\1\", \2},/p"
done

