 #!/bin/bash

tmpfile=$(mktemp /tmp/drumsynthtest.XXXXX)
files=$(/usr/bin/awk '{print $1}' <checksums.txt )

while read f t1 ; do
    line=$(./test $f timing)
    echo $line " " $t1 
done <timings.txt >>$tmpfile
awk '{printf("%d %d %3.2f%% %s\n",$2, $3, ($3/$2)*100, $1);}' <$tmpfile |less



rm $tmpfile


