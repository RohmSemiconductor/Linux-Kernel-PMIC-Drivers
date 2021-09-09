#!/bin/bash

TEST_MODULE="$1-test.ko"

DTBO="$1_test.dtbo"

if [[ $1 == "bd9576" ]] || [[ $1 == "bd9573" ]]
then
	USE_AIN=0
else
	USE_AIN=1
fi
echo "using /$DTBO"

insmod /mva_overlay.ko
if [ $USE_AIN -eq 1 ]
then
	dd if=/bbb_analog_inputs.dtbo of=/sys/kernel/mva_overlay/overlay_add bs=1M count=1
fi
dd if=/$DTBO of=/sys/kernel/mva_overlay/overlay_add bs=1M count=1
if [ $? -eq 0 ]
then
	sleep 2
	insmod $TEST_MODULE
	sleep 1
fi
