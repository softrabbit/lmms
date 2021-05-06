#!/bin/bash

for f in $(/usr/bin/awk '{print $1}' <checksums.txt ) ; do
    echo -ne '\e[K' ; ./test $f && echo -ne '\eM'
done
exit 0



