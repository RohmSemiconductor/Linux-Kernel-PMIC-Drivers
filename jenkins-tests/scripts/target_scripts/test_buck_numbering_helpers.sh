#!/bin/bash

ALL_REGULATORS=(${ALL_BUCKS[*]} ${ALL_LDOS[*]})

function buck_to_no() {
	local BUCK=$1
	echo "${BUCK: -1}"
}

function get_linear_buckno() {
	local BUCK=$1
	local BUCK_NO=$(buck_to_no $BUCK)

	if [ $(is_ldo $BUCK) -eq 1 ]
	then
		echo $(( $BUCK_NO+$LDO_START-1 ))
	else
		echo $BUCK_NO
	fi
}

