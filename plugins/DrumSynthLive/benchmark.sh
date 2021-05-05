#!/bin/bash

tmpfile=/tmp/drumsynthtimings.$(date -Iseconds)
#files=$(/usr/bin/awk '{print $1}' <timings.txt )
lines=$(wc -l timings.txt |sed 's/timings.txt//')

n=1
while read f ; do
    line=$(./test $f timing)
    echo -ne '\e[K'
    echo "("$n"/"$lines")" $line
    echo -ne '\eM'
    ((n++))
    echo $line >> $tmpfile
done <timings.txt
echo -e "\nResults saved in file $tmpfile"



