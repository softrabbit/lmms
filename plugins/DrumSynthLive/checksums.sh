#!/bin/bash
success=1
eraseprevious=0
fails=0
total=0
for f in $(/usr/bin/awk '{print $1}' <checksums.txt ) ; do
    #echo -ne '\e[K' ; ./test $f && echo -ne '\eM'
    eraseprevious=$success
    line=$(./test $f)
    success=$?

    if [ $eraseprevious -eq 0 ] ; then
	 echo -ne '\e[K'
    fi
    echo $line
    if [ $success -eq 0 ] ; then
	echo -ne '\eM'
    else
	(( fails++ ))
    fi
    (( total++ ))
done
echo -e "\n" $fails " failed out of " $total
exit 0



