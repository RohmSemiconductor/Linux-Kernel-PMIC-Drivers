#!/bin/bash

stty -F /dev/ttyUSB0 19200 cs8 cstopb -parenb

if [[ $1 == "" ]]
then
	echo "Please give 1 or 0 as argument - enable/disable"
	exit -1
fi

if [[ $1 -eq 1 ]]
then
	printf '\\\0' > /dev/ttyUSB0
fi

if [[ $1 -eq 0 ]]
then
	printf '\\\xFF' > /dev/ttyUSB0
fi

