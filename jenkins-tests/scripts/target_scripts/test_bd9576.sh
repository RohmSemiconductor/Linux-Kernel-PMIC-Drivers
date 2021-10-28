#!/bin/bash

#DEBUG_LOG=/dev/null
DEBUG_LOG=/testlogs/bd9576_test_log.txt
VOLT_LOG=/testlogs/bd9576_volt.log

VOUT_ENABLE_CONTROL=0

function init_logs() {
local D=`date`
local D2=`date -Iseconds`

#tar -cvzf /oldtestlogs/bd71847_$D2.tar.gz "$DEBUG_LOG" "$VOLT_LOG" || exit -1
echo "********************************************" > $DEBUG_LOG
echo $D >> $DEBUG_LOG
echo "" >> $DEBUG_LOG

echo "#$D" > "$VOLT_LOG"
echo "#Set	measured" >> "$VOLT_LOG"
}

#VoutS1 can't be tuned - hence only 5 regulators here
ALL_BUCKS=(buck1 buck2 buck3 buck4 buck5)
ALL_REGULATORS=(${ALL_BUCKS[*]})

DTS_PATH=/proc/device-tree/ocp/interconnect\@48000000/segment\@0/target-module\@2a000/i2c\@0/pmic\@30/regulators
AINPATH=/sys/bus/iio/devices/iio\:device0

#BUCK_DISABLE_REG=(0 0x05 0x06 0x09 0x0a 0x0b 0x0c 0x18 0x19 0x1a 0x1b 0x1c 0x1d)
VOLT_REGS=(0 0x50 0x53 0x56 0x59 0x5c)
VOLT_MASKS=(0 0x87 0x87 0x9f 0x9f 0x87)

BUCK_VOLT_MULTIPLIER=1000

BUCK1_VOLTAGES=(5000	5100	5200	5300	5400	5500	5500	5500	5000	4900	4800	4700	4600	4500	4500	4500)
BUCK1_VOLT_REGVAL=(0x0	0x1	0x2	0x3	0x4	0x5	0x6	0x7	0x80	0x81	0x82	0x83	0x84	0x85	0x86	0x87)

BUCK2_VOLTAGES=(1800	1820	1840	1860	1880	1900	1920	1940	1800	1780	1760	1740	1720	1700	1680	1660)
BUCK2_VOLT_REGVAL=(0x0	0x1	0x2	0x3	0x4	0x5	0x6	0x7	0x80	0x81	0x82	0x83	0x84	0x85	0x86	0x87)

#Buck 3 voltages can be either tuned from 1350 or 1500 mV. Let's skip this for now....
BUCK3_VOLTAGES=(1800	1820	1840	1860	1880	1900	1920	1940	1800	1780	1760	1740	1720	1700	1680	1660)
BUCK3_VOLT_REGVAL=(0x0	0x1	0x2	0x3	0x4	0x5	0x6	0x7	0x80	0x81	0x82	0x83	0x84	0x85	0x86	0x87)

BUCK_OV_REGS=(0 0x51 0x54 0x57 0x5a 0x5d)
BUCK_UV_REGS=(0 0x52 0x55 0x58 0x5b 0x5e)
BUCK_OV_MASK=0x7f
BUCK1_OV_LIMITS=(0)
BUCK234_OV_LIMITS=(0)
BUCK5_OV_LIMITS=(0)
BUCK1_TEMP_LIMITS=(0)

function prepare_ov_limits()
{
	local START_VOLTAGE=$(($1*1000))
	local LOW_NOT_CLIP_REGVAL=$2
	local INC_STEP=$(($3*1000))
	local HI_CLIPPED_REGVAL=$4
	local MAX_REGVAL=$5
	local END_VOLTAGE=$(($6*1000))
	local -n ARRAY=$7


	for (( regval=1 ; regval < $LOW_NOT_CLIP_REGVAL ; regval++ ))
	do
		ARRAY[$regval]=$START_VOLTAGE
	done

	for (( regval=$LOW_NOT_CLIP_REGVAL ; regval < $HI_CLIPPED_REGVAL ; regval++ ))
	do
		ARRAY[$regval]=$(($START_VOLTAGE+($regval-($LOW_NOT_CLIP_REGVAL-1))*$INC_STEP))
		echo ARRAY[$regval] = ${ARRAY[$regval]}
	done

	for (( regval=$HI_CLIPPED_REGVAL ; regval < $MAX_REGVAL ; regval++ ))
	do
		ARRAY[$regval]=$END_VOLTAGE
	done
}

function prepare_buck1_ov_limits()
{
	local START_VOLTAGE=225
	local LOW_NOT_CLIP_REGVAL=0x2d
	local INC_STEP=5
	local HI_CLIPPED_REGVAL=0x55
	local MAX_REGVAL=0x7f
	local END_VOLTAGE=0x425

	prepare_ov_limits $START_VOLTAGE $LOW_NOT_CLIP_REGVAL $INC_STEP $HI_CLIPPED_REGVAL $MAX_REGVAL $END_VOLTAGE BUCK1_OV_LIMITS
}

function prepare_buck234_ov_limits()
{
	local START_VOLTAGE=17
	local LOW_NOT_CLIP_REGVAL=0x11
	local INC_STEP=1
	local HI_CLIPPED_REGVAL=0x6e
	local MAX_REGVAL=0x7f
	local END_VOLTAGE=110

	prepare_ov_limits $START_VOLTAGE $LOW_NOT_CLIP_REGVAL $INC_STEP $HI_CLIPPED_REGVAL $MAX_REGVAL $END_VOLTAGE BUCK234_OV_LIMITS
}

function prepare_buck5_ov_limits()
{
	local START_VOLTAGE=34
	local LOW_NOT_CLIP_REGVAL=0x11
	local INC_STEP=2
	local HI_CLIPPED_REGVAL=0x6e
	local MAX_REGVAL=0x7f
	local END_VOLTAGE=220

	prepare_ov_limits $START_VOLTAGE $LOW_NOT_CLIP_REGVAL $INC_STEP $HI_CLIPPED_REGVAL $MAX_REGVAL $END_VOLTAGE BUCK5_OV_LIMITS
	echo "BUCK5 OVD at 0x11 is ${BUCK5_OV_LIMITS[0x11]}"
}

#List buck voltages in units of millivolts to make it easier to read the values
#Multiply them to get microvolts as driver expects

#declare -a BUCK1_VOLT_REGVAL
#declare -a BUCK2_VOLT_REGVAL
#declare -a BUCK5_VOLT_REGVAL
#declare -a BUCK6_VOLT_REGVAL

#Bucks which are connected to BBB AIN for voltage measurement
#MEASURETEST_BUCKS=(buck1 buck3 buck6 ldo4)
MEASURETEST_BUCKS=(none)
#AIN pins matching BUCKs abowe
#MEASURETEST_AINS=(0 1 2 4)
#MEASURETEST_AINS=(0 1)
MEASURETEST_AINS=(none)
#Scaling value to get microvolts out of raw AIN value.
MEASURETEST_SCALER_MUL=1800000
MEASURETEST_SCALER_DIV=4096
#tolerance limit microvolts
#MEASURETEST_TOLEERANCE=50000

function err_out() {
	echo "HANKALA HOMMA" |tee -a $DEBUG_LOG
	echo "Regulator summary:"
	cat /sys/kernel/debug/regulator/regulator_summary |tee -a $DEBUG_LOG
	exit -1
}

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

VOLTAGE_ARRAY_NAMES=(x BUCK1_VOLTAGES BUCK2_VOLTAGES BUCK3_VOLTAGES)
VOLT_REG_ARRAY_NAMES=(x BUCK1_VOLT_REGVAL BUCK2_VOLT_REGVAL BUCK3_VOLT_REGVAL)

function get_v_from_reg()
{
	local BUCK=$1
	local UV=$2
	local BUCK_NO=$(get_linear_buckno $BUCK)
	local REGVAL
	local REG
	local MASK=$BUCK_OV_MASK

	if [ $UV -eq 1 ]
	then
		REG=${BUCK_UV_REGS[$BUCK_NO]}
	else
		REG=${BUCK_OV_REGS[$BUCK_NO]}
	fi

	if [ $BUCK_NO -gt 5 ]
	then
		echo 0
		return 0
	fi

	REGVAL=$(i2cget -f -y 2 0x30 $REG)
	REGVAL=$(($REGVAL & $BUCK_OV_MASK))

	if [ $BUCK_NO -eq 1 ]
	then
		echo ${BUCK1_OV_LIMITS[$REGVAL]}
		return 0
	fi

	if [ $BUCK_NO -lt 5 ]
	then
		echo ${BUCK234_OV_LIMITS[$REGVAL]}
		return 0
	fi

	echo ${BUCK5_OV_LIMITS[$REGVAL]}
}

function get_ov_from_reg()
{
	local UV=0
	get_v_from_reg $1 $UV 
}

function get_uv_from_reg()
{
	local UV=1
	get_v_from_reg $1 $UV 
}

function get_driver_voltage()
{

	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $BUCK)

	local BUCK_CTRL_FILE=/bd9576_test/regulators/${BUCK}_set
	cat $BUCK_CTRL_FILE
}

function buck_check_volts() {
	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $BUCK)

	local BUCK_VOLT_MASK=${VOLT_MASKS[$BUCK_NO]}
	local RARRAY_NAMES=VOLT_REG_ARRAY_NAMES[$BUCK_NO]
	local VARRAY_NAMES=VOLTAGE_ARRAY_NAMES[$BUCK_NO]
	local i=0
	local VARRAY
	local RARRAY_VAL
	local regval
	local exp_regval
	local BUCK_CTRL_FILE=/bd9576_test/regulators/${BUCK}_set
	local BUCK_VOLT_REG=${VOLT_REGS[$BUCK_NO]}
	local SUCCESS

#	local STATUS=$(is_enabled $BUCK)
	local rv
	local volt
	local skipped=0

# we test only first 3 bucks for now.
	if [ $BUCK_NO -gt 3 ]
	then
		return 0
	fi
	echo "Volt testing $BUCK" |tee -a $DEBUG_LOG

	local ORIG_VOLT=$(cat $BUCK_CTRL_FILE)

	echo "Check reg $BUCK_VOLT_REG" >> $DEBUG_LOG
#
# TODO: Change the bus number to come from a variable. We need to think
# how to deliver bus information from configs to target. Or read it from DT.
# (Or hard-code to match jenkins setup - but even that benefits from single
# bus variable)
#
	regval=`i2cget -y -f 2 0x30 $BUCK_VOLT_REG`
	echo "read $regval"  >> $DEBUG_LOG
	regval=$(( $regval & $BUCK_VOLT_MASK ))

	eval VARRAY=${!VARRAY_NAMES}[@]
	for volt in ${!VARRAY}
	do
		local GET_VOLT
		eval RARRAY_VAL=${!RARRAY_NAMES}[$i]
		exp_regval=$(( ${!RARRAY_VAL} ))

		if [ $ORIG_VOLT -eq $(( $volt*$BUCK_VOLT_MULTIPLIER )) ]
		then
			if [ $exp_regval -ne $regval ]
			then
				echo "$BUCK: Unexpected voltage register value $regval - expected $exp_regval (voltage $(( $volt*$BUCK_VOLT_MULTIPLIER )) )" |tee -a $DEBUG_LOG
				err_out
			fi
			break
		fi
	done

	echo "\"$BUCK set\"	\"$BUCK measured\"" >> $VOLT_LOG
	for b in ${MEASURETEST_BUCKS[@]}
	do
		if [ "$b" = "$BUCK" ]
		then
			local VOLT=$(get_ain_volt "$BUCK")
			if [ $(( $volt*$BUCK_VOLT_MULTIPLIER )) -gt $(( $VOLT+$MEASURETEST_TOLEERANCE )) ] || [ $(( $volt*$BUCK_VOLT_MULTIPLIER )) -lt $(( $VOLT-$MEASURETEST_TOLEERANCE )) ]
			then
				echo "measured voltage $VOLT does not match set voltage $(( $volt*$BUCK_VOLT_MULTIPLIER )) - AND I DON'T CARE" |tee -a $DEBUG_LOG
				echo "comparison range: set $(( $volt*$BUCK_VOLT_MULTIPLIER )) bigger than $VOLT+$MEASURETEST_TOLEERANCE ($(( $VOLT+$MEASURETEST_TOLEERANCE ))) or smaller than $VOLT-$MEASURETEST_TOLEERANCE ($(( $VOLT-$MEASURETEST_TOLEERANCE )))" |tee -a $DEBUG_LOG
				echo "$(( $volt*$BUCK_VOLT_MULTIPLIER ))	$VOLT" >> $VOLT_LOG
#				err_out
			else
				echo "$(( $volt*$BUCK_VOLT_MULTIPLIER ))	$VOLT" >> $VOLT_LOG
				echo "$BUCK: measured voltage $VOLT matches set voltage $(( $volt*$BUCK_VOLT_MULTIPLIER ))" >> $DEBUG_LOG
			fi
		fi
	done
}

function is_enabled() {
	local BUCK=$1
	local bar
	local RV

	cat /bd9576_test/regulators/${BUCK}_en | (read -n1 bar; echo $bar)
	RV=$?
	return $RV
}

function buck_to_no() {
	local BUCK=$1
	echo "${BUCK: -1}"
}

function get_linear_buckno() {
	local BUCK=$1
	local BUCK_NO=$(buck_to_no $BUCK)

	echo $BUCK_NO
}

#
# DT-node looks like this.
# We want to support reading the set protection limits
# and the test should check the register value matches set
# limit.
#
#pmic: pmic@30 {
#	compatible = "rohm,bd9576";
#	reg = <0x30>;
#	/* Let's try using GPIO1_29 as irq pin */
#	interrupt-parent = <&gpio1>;
#	/* GPIO 1_29 - (connector 8, pin 26) for irq */
#	interrupts = <29 8>;
#	rohm,vout1-en-low;
#	rohm,vout1-en-gpios = <&gpio2 6 0>;
#	rohm,ddr-sel-low;
#	rohm,watchdog-enable-gpios = <&gpio2 7 0>;
#	rohm,watchdog-ping-gpios = <&gpio2 8 0>;
#	hw_margin_ms = <30>;
#	rohm,hw-margin-min-ms = <4>;
#
#	regulators {
#		#address-cells = <1>;
#		#size-cells = <0>;
#		boost1: regulator-vd50 {
#			regulator-name = "VD50";
#			regulator-ov-protection-microvolt = <1>;
#			regulator-ov-error-microvolt = <230000>;
#			regulator-uv-protection-microvolt = <1>;
#			regulator-uv-error-microvolt = <230000>;
#			regulator-temp-protection-kelvin = <1>;
#			regulator-temp-warn-kelvin = <0>;
#		};
#		buck1: regulator-vd18 {
#			regulator-name = "VD18";
#			regulator-ov-protection-microvolt = <1>;
#			regulator-ov-error-microvolt = <18000>;
#			regulator-uv-protection-microvolt = <1>;
#			regulator-uv-error-microvolt = <18000>;
#			regulator-temp-protection-kelvin = <1>;
#			regulator-temp-warn-kelvin = <1>;
#		};
#		buck2: regulator-vdddr {
#			regulator-name = "VDDDR";
#			regulator-ov-protection-microvolt = <1>;
#			regulator-ov-warn-microvolt = <18000>;
#			regulator-uv-protection-microvolt = <1>;
#			regulator-uv-warn-microvolt = <18000>;
#			regulator-temp-protection-kelvin = <1>;
#			regulator-temp-error-kelvin = <0>;
#		};
#		buck3: regulator-vd10 {
#			regulator-name = "VD10";
#			regulator-ov-protection-microvolt = <1>;
#			regulator-ov-warn-microvolt = <18000>;
#			regulator-uv-protection-microvolt = <1>;
#			regulator-uv-error-microvolt = <18000>;
#			regulator-temp-protection-kelvin = <1>;
#			regulator-temp-warn-kelvin = <1>;
#		};
#		ldo: regulator-voutl1 {
#			regulator-name = "VOUTL1";
#			regulator-ov-protection-microvolt = <1>;
#			regulator-ov-error-microvolt = <36000>;
#			regulator-uv-protection-microvolt = <1>;
#			regulator-uv-error-microvolt = <34000>;
#			regulator-temp-protection-kelvin = <1>;
#			regulator-temp-warn-kelvin = <0>;
#		};
#
#		sw: regulator-vouts1 {
#			regulator-name = "VOUTS1";
#			regulator-oc-protection-microamp = <0>;
#			regulator-oc-error-microamp = <200000>;
#			regulator-oc-warn-microamp = <200000>;
#		};

function get_u32dt_value()
{
	local FULL_PROP_PATH=$1
	echo '0x'`hexdump -e '4/1 "%02x"' $FULL_PROP_PATH`
}

function get_limit_prop()
{
	local FULL_PROP1_PATH=$1
	local FULL_PROP2_PATH=$2

	if [ -f $FULL_PROP1_PATH ]
	then
		get_u32dt_value $FULL_PROP1_PATH
		return 0
	fi
	if [ -f $FULL_PROP2_PATH ]
	then
		get_u32dt_value $FULL_PROP2_PATH
		return 0
	fi

	echo -1
}

function get_regulator_dt_node()
{
	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $BUCK)
	local BUCK_DTS_NAMES=(regulator-vd50 regulator-vd18 regulator-vdddr regulator-vd10 regulator-voutl1 regulator-vouts1)
	local DTS_FOLDER="$DTS_PATH"/${BUCK_DTS_NAMES[$(($BUCK_NO-1))]}

	echo $DTS_FOLDER
}

function ov_err_w_limit()
{
	local BUCK=$1
	local DTS_NODE=$(get_regulator_dt_node $BUCK)

	get_limit_prop "$DTS_NODE/regulator-ov-error-microvolt" "$DTS_NODE/regulator-ov-warn-microvolt"
}

function uv_err_w_limit()
{
	local BUCK=$1
	local DTS_NODE=$(get_regulator_dt_node $BUCK)

	get_limit_prop "$DTS_NODE/regulator-uv-error-microvolt" "$DTS_NODE/regulator-uv-warn-microvolt"
}

function temp_err_w_limit()
{
	local BUCK=$1
	local DTS_NODE=$(get_regulator_dt_node $BUCK)

	get_limit_prop "$DTS_NODE/regulator-temp-error-microvolt" "$DTS_NODE/regulator-temp-warn-microvolt"
}

function ocw_err_w_limit()
{
	local BUCK=$1
	local DTS_NODE=$(get_regulator_dt_node $BUCK)

	get_limit_prop "$DTS_NODE/regulator-oc-error-microvolt" "$DTS_NODE/regulator-oc-warn-microvolt"
}


function get_buck_controls() {
	if [ -f $DTS_PATH/../rohm,vout1-en-low ] && [ -f $DTS_PATH/../rohm,vout1-en-gpios ]
	then
		VOUT_ENABLE_CONTROL=1
	fi
}

#VOUT1 need to be checked for (mode B)
#function is_enabled_at_start() {
#	local BUCK=$1
#	local DTS_FOLDER="$DTS_PATH"/${BUCK^^}
#	local BUCK_NO=$(get_linear_buckno $BUCK)

#	echo "Checking buck $BUCK initialization values" >> $DEBUG_LOG

#	INITIAL_ENABLE[$BUCK_NO]=1
#	ALWAYS_ON[$BUCK_NO]=1
#	echo "set INITIAL_ENABLE[$BUCK_NO]=1" >> $DEBUG_LOG
#	echo "set ALWAYS_ON[$BUCK_NO]=1" >> $DEBUG_LOG
#}

function disable_buck() {
	local BUCK=$1
	local retval

	echo "echo 0 > /bd9576_test/regulators/${BUCK}_en"
	echo 0 > /bd9576_test/regulators/${BUCK}_en
	retval=$?
	return $retval
}

function enable_buck() {
	local BUCK=$1

	echo "echo 1 > /bd9576_test/regulators/${BUCK}_en"
	echo 1 > /bd9576_test/regulators/${BUCK}_en
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

function is_always_on() {
	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $b)

	echo ${ALWAYS_ON[$BUCK_NO]}
}

function get_gpio_state() {
	local GPIO_NAME=$1

	grep "$GPIO_NAME" /sys/kernel/debug/gpio >> $DEBUG_LOG
	grep "$GPIO_NAME" /sys/kernel/debug/gpio |grep lo >> /dev/null

	echo $?
}

function buck_is_enabled_raw() {
	local MASK

	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $BUCK)

	if [ $BUCK != "buck1" ]
	then
		echo 1
		return 0
	fi

	get_gpio_state vout1-en
}

function limit_check()
{
	local BUCK=$1
	local BUCK_NO=$(get_linear_buckno $BUCK)

	# This is actually not the right thing to do. We should skip reading
	# the safety registers on BD9573 even if the protection property was
	# given. Currently this works only if the DT does not contain
	# protection properties on BD9573.

	local ODT_LIMIT=$(ov_err_w_limit $BUCK)
	if [[ $ODT_LIMIT -ne -1 ]]
	then
		local OREG_LIMIT=$(get_ov_from_reg $BUCK)
	fi

	local UDT_LIMIT=$(uv_err_w_limit $BUCK)

	if [[ $UDT_LIMIT -ne -1 ]]
	then
		local UREG_LIMIT=$(get_uv_from_reg $BUCK)
	fi

	ODT_LIMIT=$((ODT_LIMIT))
	UDT_LIMIT=$((UDT_LIMIT))

	# This is not 100 % correct. The driver accepts values which are
	# not the exactly supported ones as long as the values are in the
	# range. For example 17500 uV is Ok even though IC only supports
	# 17000 or 18000. Now register limit 17000 is deemed incorrect if
	# the DT has value 17500. TODO: Fix the test. Meanwhile just use
	# only exactly supported values from the test DT. Which leaves
	# rounding untested.

	if [[ $ODT_LIMIT -ne -1 ]] && [[ $OREG_LIMIT -ne $ODT_LIMIT ]]
	then

		echo "Limit check for $BUCK failed"
		echo "ODT is $ODT_LIMIT oreg is $OREG_LIMIT"
		echo "UDT is $UDT_LIMIT ureg is $UREG_LIMIT"
		return -1
	fi

	if [[ $UDT_LIMIT -ne -1 ]] && [[ $UREG_LIMIT -ne $UDT_LIMIT ]]
	then
		echo "Limit check for $BUCK failed"
		echo "ODT is $ODT_LIMIT oreg is $OREG_LIMIT"
		echo "UDT is $UDT_LIMIT ureg is $UREG_LIMIT"
		return -1
	fi

	echo "Limit check for $BUCK succeeded"
	return 0
}

###
# Real testing starts here:
#
###
#init_volt_regvals
init_logs

get_buck_controls

prepare_buck1_ov_limits
prepare_buck234_ov_limits
prepare_buck5_ov_limits

ov_err_w_limit buck1
get_uv_from_reg buck1

for b in ${ALL_REGULATORS[*]}
do
	limit_check $b
done

#
# Test the BUCK1 enable/disable via GPIO if enable GPIO is given from DT
# See: get_buck_controls()
#
if [ $VOUT_ENABLE_CONTROL -eq 1 ]
then
	echo "enable buck 1"
	enable_buck buck1
	STATE=$(is_enabled buck1)

	if [ $STATE -ne 1 ]
	then
		echo "Failed to enable buck 1"
		err_out
	fi

	RAW_STATE=$(buck_is_enabled_raw buck1)
	if [ $RAW_STATE -ne 1 ]
	then
		grep "vout1-en" /sys/kernel/debug/gpio
		echo "Raw state for buck1 (GPIO) is $RAW_STATE(expected 1 because the framework says buck1 is enabled)"
		err_out
	fi
	echo "disable buck 1"
	disable_buck buck1
	STATE=$(is_enabled buck1)
	if [ $STATE -ne 0 ]
	then
		echo "Failed to disable buck 1"
		err_out
	fi

	RAW_STATE=$(buck_is_enabled_raw buck1)
	if [ $RAW_STATE -ne 0 ]
	then
		grep "vout1-en" /sys/kernel/debug/gpio
		echo "Raw state for buck1 (GPIO) is $RAW_STATE(expected 0 because the framework says buck1 is disabled)"
		err_out
	fi

	#enable again for potential voltage measurement
	enable_buck buck1
	STATE=$(is_enabled buck1)

	if [ $STATE -ne 1 ]
	then
		echo "Failed to re-enable buck 1"
		err_out
	fi

	RAW_STATE=$(buck_is_enabled_raw buck1)
	if [ $RAW_STATE -ne 1 ]
	then
		echo "Raw state for buck1 is $RAW_STATE (not 1) even if buck1 was just re-enabled"
		err_out
	fi

fi

#
# Check the voltage register values match voltages shown by regulator framework
#
for b in ${ALL_REGULATORS[*]}
do
	buck_check_volts $b
done

#Disable buck1 again so that next test run will succeed (we should not leave buck1 refcount increased or test will fail)
disable_buck buck1

#Then WDG test - check WDG gets enabled
WDG_STATE=$(get_gpio_state watchdog-enable)
if [ $WDG_STATE -eq 0 ]
then
	echo "Watchdog was stopped after start-up!"
fi

#Let's enable WDG:
sleep 2 >  /dev/watchdog2 &

WDG_STATE=$(get_gpio_state watchdog-enable)
if [ $WDG_STATE -ne 1 ]
then
	echo "Watchdog state was $WDG_STATE  (stopped) after opening WDG - expected 1 (started)"
	err_out
fi

sleep 3

WDG_STATE=$(get_gpio_state watchdog-enable)
if [ $WDG_STATE -ne 1 ]
then
	echo "Final watchdog state was $WDG_STATE - expected 1 as driver sets WDIOF_MAGICCLOSE"
	err_out
fi

echo "TESTS PASSED" |tee -a $DEBUG_LOG
echo "HELPPO HOMMA" |tee -a $DEBUG_LOG
sync


