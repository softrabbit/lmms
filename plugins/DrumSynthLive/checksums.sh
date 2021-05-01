#!/bin/bash

tmpfile=$(mktemp /tmp/drumsynthtest.XXXXX)

for f in $(/usr/bin/awk '{print $1}' <checksums.txt ) ; do
    ./test $f 
done >>$tmpfile
if /usr/bin/diff checksums.txt $tmpfile ; then
    echo "Checksums OK"
else
    echo "Checksums differ:"
    /usr/bin/diff checksums.txt $tmpfile
fi
rm $tmpfile


