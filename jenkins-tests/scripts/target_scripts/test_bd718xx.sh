#!/bin/bash

source test_generic_pre.sh
source test_generic_enable_helpers.sh
source test_generic_of_helpers.sh

AINPATH=/sys/bus/iio/devices/iio\:device0

for _bck_ in ${ALL_REGULATORS[*]}
do
	id=$(get_linear_buckno $_bck_)
	INITIAL_ENABLE[$id]=0
	ALWAYS_ON[$id]=0
	HW_CTRL[$id]=0
done
#INITIAL_ENABLE=(0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0)
#ALWAYS_ON=(0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0)
#HW_CTRL=(0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0)

#Scaling value to get microvolts out of raw AIN value.
MEASURETEST_SCALER_MUL=1800000
MEASURETEST_SCALER_DIV=4096
#tolerance limit microvolts
MEASURETEST_TOLEERANCE=50000

function get_ain() {
	local BUCK=$1
	for id in ${!MEASURETEST_BUCKS[*]}
	do
		if [ ${MEASURETEST_BUCKS[$id]} = "$BUCK" ]
		then
			echo ${MEASURETEST_AINS[$id]}
		fi
	done
}

function get_ain_volt() {
	local BUCK=$1
	local AIN=$(get_ain $BUCK)
	local AIN_FILE="$AINPATH/in_voltage""$AIN""_raw"
	local RAW_VOLT=$(cat "$AIN_FILE")

	echo $(( $MEASURETEST_SCALER_MUL*$RAW_VOLT/$MEASURETEST_SCALER_DIV ))
}

function is_supporting_dvs() {
	local DVS_SUPPORTED=0
	local b
	for b in ${DVS_BUCKS[@]}
	do
		if [ "$1" = "$b" ]
		then
			DVS_SUPPORTED=1
		fi
	done
	echo $DVS_SUPPORTED
}

function buck_set_n_check_volts() {
	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $BUCK)

	local BUCK_VOLT_MASK=${BUCK_VOLT_MASKS[$BUCK_NO]}
	local RARRAY_NAMES=VOLT_REG_ARRAY_NAMES[$BUCK_NO]
	local VARRAY_NAMES=VOLTAGE_ARRAY_NAMES[$BUCK_NO]
	local i=0
	local VARRAY
	local RARRAY_VAL
	local regval
	local exp_regval
	local BUCK_CTRL_FILE=/"$IC_NAME"_test/regulators/${BUCK}_set
	local BUCK_VOLT_REG=${BUCK_VOLT_REGS[$BUCK_NO]}
	local SUCCESS

	local STATUS=$(is_enabled $BUCK)
	local SUPPORT_DVS=$(is_supporting_dvs $BUCK)
	local VOLTAGE_SET_FORBIDDEN=0
	local rv
	local volt
	local skipped=0

	if  [ $STATUS -eq 1 ]
	then
		if [ $SUPPORT_DVS -eq 0 ]
		then
			VOLTAGE_SET_FORBIDDEN=1
		fi
	fi

	echo "Volt testing $BUCK" |tee -a $DEBUG_LOG

	eval VARRAY=${!VARRAY_NAMES}[@]
	for volt in ${!VARRAY}
	do
		local ORIG_VOLT=$(cat $BUCK_CTRL_FILE)
		local GET_VOLT
		eval RARRAY_VAL=${!RARRAY_NAMES}[$i]
		exp_regval=$(( ${!RARRAY_VAL} ))

		if [ $ORIG_VOLT -eq $(( $volt*$BUCK_VOLT_MULTIPLIER )) ]
		then
			if [ $i -eq 0 ]
			then
				skipped=1
			fi
			echo "value $ORIG_VOLT already set - skipping test"
			i=$(( $i+1 ))
			continue
		fi

		echo "Get old volts" >> $DEBUG_LOG
		echo "Setting volts: echo $(( $volt*$BUCK_VOLT_MULTIPLIER )) > $BUCK_CTRL_FILE" >> $DEBUG_LOG
		echo $(( $volt*$BUCK_VOLT_MULTIPLIER )) $(( $volt*$BUCK_VOLT_MULTIPLIER )) > $BUCK_CTRL_FILE
		SUCCESS=$?

		GET_VOLT=$(cat $BUCK_CTRL_FILE)

		if [ $SUCCESS -ne 0 ]
		then
			echo "Setting failed" >> $DEBUG_LOG
			if [ $VOLTAGE_SET_FORBIDDEN -eq 1 ]
			then
				echo "$BUCK: is enabled and no DVS support => failure ($SUCCESS) was expected" >> $DEBUG_LOG
			else
				echo "$BUCK: Voltage $(( $volt*$BUCK_VOLT_MULTIPLIER )) setting FAILED ( $SUCCESS )" |tee -a $DEBUG_LOG
				err_out
			fi
		else
			echo "Setting succeeded" >> $DEBUG_LOG

			if [ $GET_VOLT -ne $(( $volt*$BUCK_VOLT_MULTIPLIER )) ]
			then
				echo "$BUCK: Get volts $GET_VOLT does not match volts which were set" |tee -a $DEBUG_LOG
				err_out
			fi

			echo "Check reg $BUCK_VOLT_REG" >> $DEBUG_LOG 
			regval=`i2cget -y -f $I2C_BUS $I2C_SLAVE $BUCK_VOLT_REG`
			echo "read $regval"  >> $DEBUG_LOG
			regval=$(( $regval & $BUCK_VOLT_MASK ))
			echo "read and masked regval $regval, expected $exp_regval" >> $DEBUG_LOG

			if [ $VOLTAGE_SET_FORBIDDEN -eq 1 ]
			then
				echo "$BUCK: supports DVS (DVS_SUPPORT $SUPPORT_DVS). Status is enabled ($STATUS)" |tee -a $DEBUG_LOG
				echo "$BUCK: supports DVS (DVS_SUPPORT $SUPPORT_DVS). Status is enabled ($STATUS)" |tee -a $DEBUG_LOG
				echo "$BUCK: Voltage setting should have failed with EBUSY (16)" |tee -a $DEBUG_LOG
# New Kernels allow changing all of the bucks and LDOs on BD71847. (Eg, all are DVS-capable).
# So this check may fail if DVS_BUCKS is incorrectly set.
				err_out
			fi

			if [ $regval -ne $exp_regval ]
			then
				echo "$BUCK: Unexpected voltage register value after volt setting" |tee -a $DEBUG_LOG
				echo "Set voltage $(( $volt*$BUCK_VOLT_MULTIPLIER )) Expected regval $exp_regval, read regval $regval (register $BUCK_VOLT_REG - mask $BUCK_VOLT_MASK) used indexes, volt/reg $i, BUCK/LDO $BUCK_NO" |tee -a $DEBUG_LOG
				err_out
			fi
			#voltage_measure
			for b in ${MEASURETEST_BUCKS[@]}
			do
				if [ "$b" = "$BUCK" ]
				then
					if [ $skipped -eq 1 ] && [ $i -eq 1 ]
					then
						echo "" >> $VOLT_LOG
						echo "" >> $VOLT_LOG
						echo "\"$BUCK set\"	\"$BUCK measured\"" >> $VOLT_LOG
					fi

					if [ $i -eq 0 ]
					then
						echo "" >> $VOLT_LOG
						echo "" >> $VOLT_LOG
						echo "\"$BUCK set\"	\"$BUCK measured\"" >> $VOLT_LOG
					fi
					enable_buck "$BUCK"
					local VOLT=$(get_ain_volt "$BUCK")
					if [ $(( $volt*$BUCK_VOLT_MULTIPLIER )) -gt $(( $VOLT+$MEASURETEST_TOLEERANCE )) ] || [ $(( $volt*$BUCK_VOLT_MULTIPLIER )) -lt $(( $VOLT-$MEASURETEST_TOLEERANCE )) ]
					then
						echo "measured voltage $VOLT does not match set voltage $(( $volt*$BUCK_VOLT_MULTIPLIER )) - but I DON'T CARE" |tee -a $DEBUG_LOG
						echo "comparison range: set $(( $volt*$BUCK_VOLT_MULTIPLIER )) bigger than $VOLT+$MEASURETEST_TOLEERANCE ($(( $VOLT+$MEASURETEST_TOLEERANCE ))) or smaller than $VOLT-$MEASURETEST_TOLEERANCE ($(( $VOLT-$MEASURETEST_TOLEERANCE )))" |tee -a $DEBUG_LOG
						echo "$(( $volt*$BUCK_VOLT_MULTIPLIER ))	$VOLT" >> $VOLT_LOG
						disable_buck "$BUCK"
#						err_out
					else
						echo "$(( $volt*$BUCK_VOLT_MULTIPLIER ))	$VOLT" >> $VOLT_LOG
						disable_buck "$BUCK"
						echo "$BUCK: measured voltage $VOLT matches set voltage $(( $volt*$BUCK_VOLT_MULTIPLIER ))" >> $DEBUG_LOG
					fi
				fi
			done
		fi
		i=$(( $i+1 ))
	done
}

function is_parent() {
	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $b)

	echo ${BUCK_IS_PARENT[$BUCK_NO]}
}

function is_enabled_at_start() {
	local BUCK=$1
#	local DTS_FOLDER="$DTS_PATH"/${BUCK^^}
	local BUCK_NO=$(get_linear_buckno $BUCK)

	echo "Checking buck $BUCK initialization values from $DTS_PATH" >> $DEBUG_LOG

#	if [ -f "$DTS_FOLDER/regulator-boot-on" ] 
#	then
#		echo "file $DTS_FOLDER/regulator-boot-on exists" 
#		INITIAL_ENABLE[$BUCK_NO]=1
#		echo "set INITIAL_ENABLE[$BUCK_NO]=1" >> $DEBUG_LOG
#	fi
	ALWAYS_ON=$(always_on_by_dt $BUCK)
	HW_CTRL=$(hw_controlled_by_dt $BUCK)

	if [ $ALWAYS_ON -eq 1 ] || [ ${BUCK_IS_PARENT[$BUCK_NO]} -eq 1 ]
	then
#		echo "file $DTS_FOLDER/regulator-always-on exists" 
		INITIAL_ENABLE[$BUCK_NO]=1
		ALWAYS_ON[$BUCK_NO]=1
		echo "set INITIAL_ENABLE[$BUCK_NO]=1" >> $DEBUG_LOG
		echo "set ALWAYS_ON[$BUCK_NO]=1" >> $DEBUG_LOG
	fi

	if [ $HW_CTRL -eq 1 ]
	then
#		echo "file $DTS_FOLDER/rohm,no-regulator-enable-control exists" 
		HW_CTRL[$BUCK_NO]=1
	fi

}

function is_hwcontrolled() {
	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $BUCK)

	return ${HW_CTRL[$BUCK_NO]}
}


function first_run_checks() {
	for b in ${ALL_REGULATORS[*]}
	do
		local BUCK_NO=$(get_linear_buckno $b)

		STATUS=$(is_enabled $b)

		if [ ${HW_CTRL[$BUCK_NO]} -eq 1 ]
		then
			if [ ${HW_CTRL_ON[$BUCK_NO]} -eq 2 ]
			then
				${HW_CTRL_ON[$BUCK_NO]}=$(IC_SPECIFIC_buck_on_at_hwctrl_raw $b)
			fi
			if [ $STATUS -ne ${HW_CTRL_ON[$BUCK_NO]} ]
			then
				echo "BUCK $b should be under HW control but enable status was $STATUS" |tee -a $DEBUG_LOG
				err_out
			else
				echo "BUCK $b under HW control. Enable status $STATUS as expected" |tee -a $DEBUG_LOG
			fi
			continue
		fi

		if [ $STATUS -ne ${INITIAL_ENABLE[$BUCK_NO]} ]
		then
			echo "Regulator $b initial enable (from DT) should be ${INITIAL_ENABLE[$BUCK_NO]} but status is $RAW_STATUS (even though the raw status was correctly $RAW_STATUS)" |tee -a $DEBUG_LOG
			err_out
		fi
		if [ ${NO_RAW_STATUS[$BUCK_NO]} -eq 1 ]
		then
			continue
		fi
		RAW_STATUS=$(buck_is_enabled_raw $b)
		if [ $RAW_STATUS -ne ${INITIAL_ENABLE[$BUCK_NO]} ]
		then
			echo "Regulator $b initial enable (from DT) should be ${INITIAL_ENABLE[$BUCK_NO]} but raw status is $RAW_STATUS (framework status was $STATUS)" |tee -a $DEBUG_LOG
			err_out
		fi
	done
}

function was_initially_enabled() {
	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $b)

	if [ ${INITIAL_ENABLE[$BUCK_NO]} -eq 1 ]
	then
		echo 1
	else
		echo 0
	fi
}

function buck_is_enabled_raw() {
	local MASK

	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $b)
	local REG=${BUCK_DISABLE_REG[$BUCK_NO]}
	local regval=`i2cget -y -f $I2C_BUS $I2C_SLAVE $REG`
	local RV=$?

	if [ $(is_ldo $BUCK) -eq 0 ]
	then
		MASK=$(( $BUCK_ENABLE_MASK | $BUCK_CONTROLLED_MASK ))
		echo "REG  $REG. BUCK $BUCK"  >> $DEBUG_LOG
	else
		MASK=$(( $LDO_ENABLE_MASK | $LDO_CONTROLLED_MASK ))
		echo "REG  $REG. LDO $BUCK" >> $DEBUG_LOG
	fi

	echo "buck_is_enabled_raw: BUCK_DISABLE_REG[$BUCK_NO] = ${BUCK_DISABLE_REG[$BUCK_NO]} - mask $MASK reg $REG value $regval regval & mask = $(( $regval & $MASK ))" >> $DEBUG_LOG

	if [ $(( $regval & $MASK )) -eq $MASK ]
	then
		echo 1
	else
		echo 0
	fi
	return $RV
}


###
# Real testing starts here:
#
###
IC_SPECIFIC_init
init_logs

#
# Check modules are load
#
modules_load

#Populate initial state values from DT
for foo in ${ALL_REGULATORS[*]}
do
	is_enabled_at_start $foo
done
#
# Regulator framework was changed to check for unbalanced disables/enables which
# prevents the disable-enable-disable hack.
#  
if [ -n "$1" ] 
then
	#if [ $1 -eq 1 ]
	#then
	echo "comparing regulator states to DT initialization values"
	first_run_checks
else
	echo "Skipping first run checks"
fi
#else
#	echo "Skipping first run checks - doing disable-enable-disable cycle to get known state"

#	for b in ${ALL_REGULATORS[*]}
#	do
#		echo disable $b
#		disable_buck $b
#        	echo enable $b
#        	enable_buck $b
#		echo disable $b
#		disable_buck $b
#	done
#fi

# try disabling monitoring (Just a temporary thing to avoid
# problems during test development. Should be removed later):

echo "Disabling voltage monitoring"

#Bucks and LDOs both have 80% limit monitoring
for foo in ${ALL_REGULATORS[*]}
do
	/bd_monitor_toggle.sh ${foo^^} 80 0 > /dev/null 2>&1
done

#Additionally bucks have 130% limit monitoring
for foo in ${ALL_BUCKS[*]}
do
	/bd_monitor_toggle.sh ${foo^^} 130 0 > /dev/null 2>&1
done

echo "...monitoring disabled"

# Disable all enabled regulators. Allow buck2 to stay enabled
# because it is marked as "always-on" in DT.
# I guess we should also see BUCK6 and BUCK7 staying enabled
# if LDO5 and LDO6 are enabled because of the parent-child relation.
echo "Disabling all regulators and checking they really get disabled"
for b in ${ALL_REGULATORS[*]}
do
	#HWC=$(is_hwcontrolled $b)

	is_hwcontrolled $b
	HWC=$?
	if [ $HWC -ne 0 ]
	then
		echo "$b marked HW controlled - check enable/disable does not change status"
		STATUS=$(is_enabled $b)
		enable_buck $b
		STATUSNEW=$(is_enabled $b)
		if [ $STATUS -ne $STATUSNEW ]
		then
			echo "HW controlled regulator status changed when enabling from $STATUS to $STATUSNEW"
			err_out
		fi
		echo disable $b
		STATUSNEW=$(is_enabled $b)
		if [ $STATUS -ne $STATUSNEW ]
		then
			echo "HW controlled regulator status changed when disabling from $STATUS to $STATUSNEW"
			err_out
		fi
	else
		STATUS=$(is_enabled $b)
		if [ $(is_always_on $b) -eq 0 ]
		then
			if [ $(is_parent $b) -eq 0 ]
			then
				# Do dummy enable before disable to not get unbalanced disables 
				echo dummy enable $b
				enable_buck $b
				echo disable $b
				disable_buck $b
			fi
		fi
		STATUS=$(is_enabled $b)
		if [ $STATUS -eq 1 ] && [ $(is_always_on $b) -eq 0 ]
		then
			if [ $(is_parent $b) -eq 0 ]
			then
				echo "initially enabled $(was_initially_enabled $b)"
				if [ $(was_initially_enabled $b) -eq 0 ]
				then
					echo "1.st disable: Failed to disable $b (status $STATUS)" |tee -a $DEBUG_LOG
					err_out
				else
					echo "Allowed buck $b status to be $STATUS after disable as assumed unbalanced disable (enabled at bootup by DT)" >> $DEBUG_LOG
				fi
			fi
		fi

		if [ ${NO_RAW_STATUS[$BUCK_NO]} -eq 1 ]
		then
			continue
		fi
		RAW_STATUS=$(buck_is_enabled_raw $b)
		if [ $STATUS -ne $RAW_STATUS ]
		then
			echo "Raw register status ($RAW_STATUS) and regulator status ($STATUS) does not match for $b after 1.st disable" |tee -a $DEBUG_LOG
			err_out
		fi
	fi
done
echo "...regulators disabled"

#Try enabling all regulators
echo "Enabling all regulators and checking they really get enabled"
for b in ${ALL_REGULATORS[*]}
do

	#HWC=$(is_hwcontrolled $b)

	is_hwcontrolled $b
	HWC=$?
	if [ $HWC -ne 1 ]
	then
	        echo enable $b
	        enable_buck $b
		STATUS=$(is_enabled $b)
		echo "Buck $b status $STATUS" >> $DEBUG_LOG
		if [ $STATUS -eq 0 ]
		then
			echo "Failed to enable $b" |tee -a $DEBUG_LOG
			err_out
		fi
		if [ ${NO_RAW_STATUS[$BUCK_NO]} -eq 1 ]
		then
			continue
		fi
		RAW_STATUS=$(buck_is_enabled_raw $b)
		if [ $STATUS -ne $RAW_STATUS ]
	        then
	                echo "Raw register status ($RAW_STATUS) and regulator status ($STATUS) does not match for $b after enable" |tee -a $DEBUG_LOG
			err_out
	        fi
	fi
done
echo "...regulators enabled"

echo "Disabling all regulators (again) and checking they really get disabled"
for b in ${ALL_REGULATORS[*]}
do
	echo disable $b
	disable_buck $b
	RETV=$?

	is_hwcontrolled $b
	HWC=$?
	if [ $HWC -eq 1 ]
	then
		continue
	fi
	if [ $RETV -ne 0 ]
	then
		echo "$b: Disabling FAILED" |tee -a $DEBUG_LOG
		if [ $(is_always_on $b) -eq 0 ] && [ $(is_parent $b) -eq 0 ]
		then
			echo "$b: Failure was unexpected" |tee -a $DEBUG_LOG
			err_out
		else
			echo "$b: Failure was expected" |tee -a $DEBUG_LOG
		fi
	fi 	

	STATUS=$(is_enabled $b)
	RETV=$?
	if [ $RETV -ne 0 ]
	then
		echo "$b: Failed to read status!" |tee -a $DEBUG_LOG
		err_out
	fi
	echo "Buck $b status $STATUS"
	if [ $STATUS -eq 1 ] && [ $(is_always_on $b) -eq 0 ] && [ $(is_parent $b) -eq 0 ]
	then
		echo "Final disable: Failed to disable $b" |tee -a $DEBUG_LOG
		err_out
	fi
	if [ ${NO_RAW_STATUS[$BUCK_NO]} -eq 1 ]
	then
		continue
	fi
	RAW_STATUS=$(buck_is_enabled_raw $b)
	RETV=$?
	if [ $RETV -ne 0 ]
	then
		echo "$b: Failed to read raw status!" |tee -a $DEBUG_LOG
		err_out
	fi
	if [ $STATUS -ne $RAW_STATUS ]
	then
		echo "Raw register status ($RAW_STATUS) and regulator status ($STATUS) does not match for $b after final disable" |tee -a $DEBUG_LOG
		err_out
	fi
done
echo "...regulators disabled"

#Voltage setting test
#if [ -n $2 ]
#then
#	buck_set_n_check_volts $2
#else
	for b in ${ALL_REGULATORS[*]}
	do
		buck_set_n_check_volts $b
	done
#fi

echo "TESTS PASSED" |tee -a $DEBUG_LOG
echo "HELPPO HOMMA" |tee -a $DEBUG_LOG
sync


