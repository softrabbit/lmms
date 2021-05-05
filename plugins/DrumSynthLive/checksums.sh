#!/bin/bash

tmpfile=$(mktemp /tmp/drumsynthtest.XXXXX)

for f in $(/usr/bin/awk '{print $1}' <checksums.txt ) ; do
    ./test $f 
done >>$tmpfile
if /usr/bin/diff -q checksums.txt $tmpfile ; then
    echo "Checksums OK"
else
    echo "Checksums differ:"
    /usr/bin/diff -y checksums.txt $tmpfile |less
fi
rm $tmpfile


