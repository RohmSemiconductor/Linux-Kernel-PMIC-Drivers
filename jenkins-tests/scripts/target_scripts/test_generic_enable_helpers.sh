#!/bin/bash

function is_ldo() {
	local BUCK=$1

	if [ "${BUCK:0:3}" = "ldo" ]
	then
		echo 1
	else
		echo 0
	fi
}

function is_buck() {
	local BUCK=$1

	if [ "${BUCK:0:4}" = "buck" ]
	then
		echo 1
	else
		echo 0
	fi
}


tf=$(type -t get_linear_buckno 2>/dev/null || rt=$?)
if [[ "$tf" != "function" ]]; then
#In the target these scripts are at the root. TODO: Make proper folder
#structure
	source /test_buck_numbering_helpers.sh
fi

function is_enabled() {
	local BUCK=$1
	local bar
	local RV

	cat /"$IC_NAME"_test/regulators/${BUCK}_en | (read -n1 bar; echo $bar)
	RV=$?
	return $RV
}

function disable_buck() {
	local BUCK=$1
	local retval

	echo 0 > /"$IC_NAME"_test/regulators/${BUCK}_en
	retval=$?
	return $retval
}

function enable_buck() {
	local BUCK=$1

	echo 1 > /"$IC_NAME"_test/regulators/${BUCK}_en
}

function get_voltage()
{
	local BUCK=$1
	local bar
	local RV

	cat /"$IC_NAME"_test/regulators/${BUCK}_set | (read -n1 bar; echo $bar)
	RV=$?
	return $RV
}

function set_voltage()
{
	local BUCK=$1
	local VOLTAGE=$2

	echo $VOLTAGE > /"$IC_NAME"_test/regulators/${BUCK}_set
}

