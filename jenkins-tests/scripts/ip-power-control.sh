#!/bin/bash

if [[ $1 == "" ]]
then
	echo "first parameter should be port, 1 ... 4"
fi

if [[ $2 == "" ]]
then
	echo "second parameter should be 1 or 0 (meaning on or off)"
fi


PORTCOMMAND="ioctrl -s p6$1"
PORTCOMMAND="$PORTCOMMAND=$2"

{ sleep 0.5; echo "admin"; sleep 0.5; echo "12345678"; sleep 0.5; echo "$PORTCOMMAND"; sleep 1; } | telnet 192.168.255.250

