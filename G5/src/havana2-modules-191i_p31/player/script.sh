#!/bin/bash
cat $1 | awk -f script.ask > $1.new
rm $1
mv $1.new $1
