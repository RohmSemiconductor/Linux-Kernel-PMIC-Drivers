#!/bin/bash

tf=$(type -t err_out 2>/dev/null || rt=$?)
if [[ "$tf" != "function" ]]; then
	source test_generic_pre.sh
fi
source test_buck_numbering_helpers.sh
source test_generic_enable_helpers.sh

# Call any IC specific inits - like setup test voltage tables
IC_SPECIFIC_init

# TEST1: Check the modules are load:
modules_load

for b in ${ALL_REGULATORS[*]}
do

#TEST2 see enabling works
	CAN_ENABLE=`can_enable $b`
	if [ $CAN_ENABLE -eq 1 ]
	then
		echo "enable regulator '$b'"
		enable_buck $b || err_out

		echo "Check regulator '$b' is enabled"
		regulator_check_enabled $b || err_out

#TEST3 see disabling works
		CAN_DISABLE=`can_disable $b`
		if [ $CAN_DISABLE -eq 1 ]
		then
			echo "disable regulator '$b'"
			disable_buck $b || err_out

			echo "Check regulator '$b' is disabled"
			regulator_check_disabled $b || err_out
		fi

		echo "re-enable regulator '$b'"
		enable_buck $b || err_out
	fi

#TEST4 check read voltage matches REG config
	CAN_CHECK_VOLTAGE=`can_check_voltage $b`
	if [ $CAN_CHECK_VOLTAGE -eq 1 ]
	then
		VOLTAGE=get_voltage $b
		echo "Check the read voltage $VOLTAGE for '$b' is correct"
		check_voltage $b $VOLTAGE || err_out
	fi

#TEST5 set and check the voltages
	if [ $CAN_SET_VOLTAGE -eq 1 ]
	then
		local POSSIBLE_VOLTAGES=()

		regulator_get_all_voltages $b $POSSIBLE_VOLTAGES

		for VOLT in ${POSSIBLE_VOLTAGES[*]}
		do
			echo "try setting voltage $VOLT for '$b'"
			set_voltage $b $VOLT || err_out

			if [ $CAN_CHECK_VOLTAGE -eq 1 ]
			then
				echo "Check the set voltage $VOLT for '$b' is correct"
				check_voltage $b $VOLT || err_out
			fi
		done
	fi
done

