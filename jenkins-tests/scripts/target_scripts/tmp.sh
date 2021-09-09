#!/bin/bash

BUCK_OV_LIMITS1=(0)

function test()
{
	local -n ARRAY=$1

	for (( regval=1 ; regval < 0x2d ; regval++ ))
	do
		ARRAY[$regval]=225
	done

	for (( regval=0x2d ; regval < 0x55 ; regval++ ))
	do
		ARRAY[$regval]=$((225+($regval-0x2c)*5))
		echo ARRAY[$regval] = ${ARRAY[$regval]}
	done

	for (( regval=0x55 ; regval < 0x7f ; regval++ ))
	do
		ARRAY[$regval]=425
	done

	echo OV_LIMIT[0x2c] = ${BUCK_OV_LIMITS1[0x2c]}
	echo OV_LIMIT[0x2d] = ${BUCK_OV_LIMITS1[0x2d]}
	echo OV_LIMIT[0x53] = ${BUCK_OV_LIMITS1[0x53]}
	echo OV_LIMIT[0x54] = ${BUCK_OV_LIMITS1[0x54]}
}

test BUCK_OV_LIMITS1
