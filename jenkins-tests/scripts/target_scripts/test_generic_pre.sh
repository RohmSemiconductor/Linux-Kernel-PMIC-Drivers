#!/bin/bash

DEBUG_LOG=/testlogs/"$IC_NAME"_test_log.txt
VOLT_LOG=/testlogs/"$IC_NAME"_volt.log

function init_logs() {
local D=`date`
local D2=`date -Iseconds`

echo "********************************************" > $DEBUG_LOG
echo $D >> $DEBUG_LOG
echo "" >> $DEBUG_LOG

echo "#$D" > "$VOLT_LOG"
echo "#Set	measured" >> "$VOLT_LOG"
}

function err_out() {
	echo "HANKALA HOMMA" |tee -a $DEBUG_LOG
	echo "Regulator summary:"
	cat /sys/kernel/debug/regulator/regulator_summary |tee -a $DEBUG_LOG
	exit -1
}

function modules_load() {
	local MODULES=$(lsmod)
	for m in $IC_MODULES
	do
		echo "looking for $m"
		echo $MODULES ¦grep $m || err_out
	done
}

