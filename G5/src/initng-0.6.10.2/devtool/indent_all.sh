#!/bin/bash


echo "scan for sources to indent."

SKIP="ash_netbsd_launcher/sh|ash_launcher/sh"

for file in `find . -name "*.c" -printf "%h/%f "`
do
    echo "Indenting file: $file"
    indent -ut -ci100 -cli0 -sob -bad -bap -bbb -bl -bli0 -nce -cli4 -cbi4 -ss -npcs -nprs -npsl -i4 -ts4 -lp -fc1 -c45 -nsob $file
done

echo "scan for headers to indent."
for file in `find . -name "*.h" -printf "%h/%f "`
do
    echo "Indenting file: $file"
    indent -ut -ci100 -cli0 -sob -bad -bap -bbb -bl -bli0 -nce -cli4 -cbi4 -ss -npcs -nprs -npsl -i4 -ts4 -lp -fc1 -c45 -nsob $file
done

echo "Clean up."
for file in `find . -name "*~" -printf "%h/%f "`
do
    echo "Removing file: $file"
    rm ${file}
done

exit 0
