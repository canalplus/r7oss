#!/bin/sh

mkdir -p kptrace
chmod a+w kptrace
kptrace -x /root/kptrace.conf -T /root
