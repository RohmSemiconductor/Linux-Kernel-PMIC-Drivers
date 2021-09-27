#!/bin/bash

tf=$(type -t get_linear_buckno 2>/dev/null || rt=$?)
if [[ "$tf" != "function" ]]; then
#In the target these scripts are at the root. TODO: Make proper folder
#structure
	source /test_buck_numbering_helpers.sh
fi

tf=$(type -t err_out 2>/dev/null || rt=$?)
if [[ "$tf" != "function" ]]; then
	source test_generic_pre.sh
fi

function hw_controlled_by_dt() {
	local BUCK=$1
	local DTS_FOLDER="$DTS_PATH"/${BUCK^^}
	local DTS_FOLDER2="$DTS_PATH/$BUCK"

	if [ -f "$DTS_FOLDER/rohm,no-regulator-enable-control" ] || [ -f  "$DTS_FOLDER2/rohm,no-regulator-enable-control" ]
	then
		echo 1
	else
		echo 0
	fi
}

function boot_on_by_dt() {
	local BUCK=$1
	local DTS_FOLDER="$DTS_PATH"/${BUCK^^}
	local DTS_FOLDER2="$DTS_PATH/$BUCK"

	if [ -f "$DTS_FOLDER/regulator-boot-on" ] || [ -f  "$DTS_FOLDER2/regulator-boot-on" ]
	then
		echo 1
	else
		echo 0
	fi
}

function always_on_by_dt() {
	local BUCK=$1
	local DTS_FOLDER="$DTS_PATH"/${BUCK^^}
	local DTS_FOLDER2="$DTS_PATH/$BUCK"

	if [ -f "$DTS_FOLDER/regulator-always-on" ] || [ -f  "$DTS_FOLDER2/regulator-always-on" ]
	then
		echo 1
	else
		echo 0
	fi
}

function init_on_state_by_dt() {
	if [ -d "$DTS_PATH" ]
	then
		echo "DTS folder exists"
	else
		echo "******* DTS folder $DTS_PATH not found ********"
		err_out
	fi
	for _bck_ in ${ALL_REGULATORS[*]}
	do
		id=$(get_linear_buckno $_bck_)
		ALWAYS_ON[$id]=$(always_on_by_dt $_bck_)
		if [ ${ALWAYS_ON[$id]} -eq 1 ] || [ $(boot_on_by_dt $_bck_) -eq 1 ]
		then
			INITIAL_ENABLE[$id]=1
		fi
		HW_CTRL[$id]=$(hw_controlled_by_dt $_bck_)
		echo "DT-parsed for $_bck_: always-on: ${ALWAYS_ON[$id]} initial-enable: ${INITIAL_ENABLE[$id]} hw-ctrl: ${HW_CTRL[$id]}"
	done
}

function is_always_on() {
	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $b)

	echo ${ALWAYS_ON[$BUCK_NO]}
}


