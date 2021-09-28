#!/bin/bash

echo "board $1 given"

if [ ! -f /sys/class/regulator/regulator.13/name ]
then
	echo "calling /doverlay.sh $1"
	/doverlay.sh "$1"
	if [ $? -ne 0 ]
	then
		echo "usage: ./init_re... <regulator type>"
		exit -1
	else
		echo "doverlay succeeded"
	fi
	sleep 1
fi

sleep 2
if [ ! -f /sys/kernel/mva_test/regulators/buck1_en ]
then
	insmod /"$1"-test.ko
	sleep 2
fi

if [ "$1" = bd9573 ]
then
	TARGET="bd9576"
else
	TARGET=$1
fi

if [ ! -f /"$TARGET"_test/regulators ]
then
	mkdir -p /"$TARGET"_test/regulators
fi

buck10_en   buck11_en	buck12_en   buck1_en   buck2_en   buck3_en   buck4_en	buck5_en   buck6_en   buck7_en	 buck8_en   buck9_en   wled_en
buck10_set  buck11_set	buck12_set  buck1_set  buck2_set  buck3_set  buck4_set	buck5_set  buck6_set  buck7_set  buck8_set  buck9_set  wled_set

if [ "$TARGET" = bd71815 ]
then
	ln -sf /sys/kernel/mva_test/regulators/buck1_en /"$TARGET"_test/regulators/buck1_en
	ln -sf /sys/kernel/mva_test/regulators/buck1_set /"$TARGET"_test/regulators/buck1_set
	ln -sf /sys/kernel/mva_test/regulators/buck2_en /"$TARGET"_test/regulators/buck2_en
	ln -sf /sys/kernel/mva_test/regulators/buck2_set /"$TARGET"_test/regulators/buck2_set
	ln -sf /sys/kernel/mva_test/regulators/buck3_en /"$TARGET"_test/regulators/buck3_en
	ln -sf /sys/kernel/mva_test/regulators/buck3_set /"$TARGET"_test/regulators/buck3_set
	ln -sf /sys/kernel/mva_test/regulators/buck4_en /"$TARGET"_test/regulators/buck4_en
	ln -sf /sys/kernel/mva_test/regulators/buck4_set /"$TARGET"_test/regulators/buck4_set
	ln -sf /sys/kernel/mva_test/regulators/buck5_en /"$TARGET"_test/regulators/buck5_en
	ln -sf /sys/kernel/mva_test/regulators/buck5_set /"$TARGET"_test/regulators/buck5_set
	#TODO: Fix test scripts to use either only buckXX fir all regulators - ro ldoxx for LDOs - but do not mix usage. Doing duial links here is a hack.
	ln -sf /sys/kernel/mva_test/regulators/buck6_en /"$TARGET"_test/regulators/buck6_en
	ln -sf /sys/kernel/mva_test/regulators/buck6_en /"$TARGET"_test/regulators/ldo1_en
	ln -sf /sys/kernel/mva_test/regulators/buck6_set /"$TARGET"_test/regulators/buck6_set
	ln -sf /sys/kernel/mva_test/regulators/buck6_set /"$TARGET"_test/regulators/ldo1_set
	ln -sf /sys/kernel/mva_test/regulators/buck7_en /"$TARGET"_test/regulators/buck7_en
	ln -sf /sys/kernel/mva_test/regulators/buck7_set /"$TARGET"_test/regulators/buck7_set
	ln -sf /sys/kernel/mva_test/regulators/buck7_en /"$TARGET"_test/regulators/ldo2_en
	ln -sf /sys/kernel/mva_test/regulators/buck7_set /"$TARGET"_test/regulators/ldo2_set
	ln -sf /sys/kernel/mva_test/regulators/buck8_en /"$TARGET"_test/regulators/buck8_en
	ln -sf /sys/kernel/mva_test/regulators/buck8_set /"$TARGET"_test/regulators/buck8_set
	ln -sf /sys/kernel/mva_test/regulators/buck8_en /"$TARGET"_test/regulators/ldo3_en
	ln -sf /sys/kernel/mva_test/regulators/buck8_set /"$TARGET"_test/regulators/ldo3_set
	ln -sf /sys/kernel/mva_test/regulators/buck9_en /"$TARGET"_test/regulators/buck9_en
	ln -sf /sys/kernel/mva_test/regulators/buck9_set /"$TARGET"_test/regulators/buck9_set
	ln -sf /sys/kernel/mva_test/regulators/buck9_en /"$TARGET"_test/regulators/ldo4_en
	ln -sf /sys/kernel/mva_test/regulators/buck9_set /"$TARGET"_test/regulators/ldo4_set
	ln -sf /sys/kernel/mva_test/regulators/bucl10_en /"$TARGET"_test/regulators/buck10_en
	ln -sf /sys/kernel/mva_test/regulators/buck10_set /"$TARGET"_test/regulators/buck10_set
	ln -sf /sys/kernel/mva_test/regulators/buck11_en /"$TARGET"_test/regulators/buck11_en
	ln -sf /sys/kernel/mva_test/regulators/buck11_set /"$TARGET"_test/regulators/buck11_set
	ln -sf /sys/kernel/mva_test/regulators/buck10_en /"$TARGET"_test/regulators/ldo5_en
	ln -sf /sys/kernel/mva_test/regulators/buck10_set /"$TARGET"_test/regulators/ldo5_set
o	ln -sf /sys/kernel/mva_test/regulators/wled_en /"$TARGET"_test/regulators/wled_en
	ln -sf /sys/kernel/mva_test/regulators/wled_set /"$TARGET"_test/regulators/wled_set
fi
if [ "$TARGET" = bd9576 ]
then
	ln -sf /sys/kernel/mva_test/regulators/buck1_en /"$TARGET"_test/regulators/buck1_en
	ln -sf /sys/kernel/mva_test/regulators/buck1_set /"$TARGET"_test/regulators/buck1_set
	ln -sf /sys/kernel/mva_test/regulators/buck2_en /"$TARGET"_test/regulators/buck2_en
	ln -sf /sys/kernel/mva_test/regulators/buck2_set /"$TARGET"_test/regulators/buck2_set
	ln -sf /sys/kernel/mva_test/regulators/buck3_en /"$TARGET"_test/regulators/buck3_en
	ln -sf /sys/kernel/mva_test/regulators/buck3_set /"$TARGET"_test/regulators/buck3_set
	ln -sf /sys/kernel/mva_test/regulators/buck4_en /"$TARGET"_test/regulators/buck4_en
	ln -sf /sys/kernel/mva_test/regulators/buck4_set /"$TARGET"_test/regulators/buck4_set
	ln -sf /sys/kernel/mva_test/regulators/buck5_en /"$TARGET"_test/regulators/buck5_en
	ln -sf /sys/kernel/mva_test/regulators/buck5_set /"$TARGET"_test/regulators/buck5_set
	ln -sf /sys/kernel/mva_test/regulators/buck6_en /"$TARGET"_test/regulators/buck6_en
	ln -sf /sys/kernel/mva_test/regulators/buck6_set /"$TARGET"_test/regulators/buck6_set
fi

if [ "$1" = bd71837 ]
then

	ln -sf /sys/kernel/mva_test/regulators/buck1_en /"$TARGET"_test/regulators/buck1_en
	ln -sf /sys/kernel/mva_test/regulators/buck1_set /"$TARGET"_test/regulators/buck1_set
	ln -sf /sys/kernel/mva_test/regulators/buck2_en /"$TARGET"_test/regulators/buck2_en
	ln -sf /sys/kernel/mva_test/regulators/buck2_set /"$TARGET"_test/regulators/buck2_set
	ln -sf /sys/kernel/mva_test/regulators/buck3_en /"$TARGET"_test/regulators/buck3_en
	ln -sf /sys/kernel/mva_test/regulators/buck3_set /"$TARGET"_test/regulators/buck3_set
	ln -sf /sys/kernel/mva_test/regulators/buck4_en /"$TARGET"_test/regulators/buck4_en
	ln -sf /sys/kernel/mva_test/regulators/buck4_set /"$TARGET"_test/regulators/buck4_set
	ln -sf /sys/kernel/mva_test/regulators/buck5_en /"$TARGET"_test/regulators/buck5_en
	ln -sf /sys/kernel/mva_test/regulators/buck5_set /"$TARGET"_test/regulators/buck5_set
	ln -sf /sys/kernel/mva_test/regulators/buck6_en /"$TARGET"_test/regulators/buck6_en
	ln -sf /sys/kernel/mva_test/regulators/buck6_set /"$TARGET"_test/regulators/buck6_set
	ln -sf /sys/kernel/mva_test/regulators/buck7_en /"$TARGET"_test/regulators/buck7_en
	ln -sf /sys/kernel/mva_test/regulators/buck7_set /"$TARGET"_test/regulators/buck7_set
	ln -sf /sys/kernel/mva_test/regulators/buck8_en /"$TARGET"_test/regulators/buck8_en
	ln -sf /sys/kernel/mva_test/regulators/buck8_set /"$TARGET"_test/regulators/buck8_set


	ln -sf /sys/kernel/mva_test/regulators/buck9_en /"$TARGET"_test/regulators/ldo1_en
	ln -sf /sys/kernel/mva_test/regulators/buck9_set /"$TARGET"_test/regulators/ldo1_set
	ln -sf /sys/kernel/mva_test/regulators/buck10_en /"$TARGET"_test/regulators/ldo2_en
	ln -sf /sys/kernel/mva_test/regulators/buck10_set /"$TARGET"_test/regulators/ldo2_set
	ln -sf /sys/kernel/mva_test/regulators/buck11_en /"$TARGET"_test/regulators/ldo3_en
	ln -sf /sys/kernel/mva_test/regulators/buck11_set /"$TARGET"_test/regulators/ldo3_set
	ln -sf /sys/kernel/mva_test/regulators/buck12_en /"$TARGET"_test/regulators/ldo4_en
	ln -sf /sys/kernel/mva_test/regulators/buck12_set /"$TARGET"_test/regulators/ldo4_set
	ln -sf /sys/kernel/mva_test/regulators/buck13_en /"$TARGET"_test/regulators/ldo5_en
	ln -sf /sys/kernel/mva_test/regulators/buck13_set /"$TARGET"_test/regulators/ldo5_set
	ln -sf /sys/kernel/mva_test/regulators/buck14_en /"$TARGET"_test/regulators/ldo6_en
	ln -sf /sys/kernel/mva_test/regulators/buck14_set /"$TARGET"_test/regulators/ldo6_set
	ln -sf /sys/kernel/mva_test/regulators/buck15_en /"$TARGET"_test/regulators/ldo7_en
	ln -sf /sys/kernel/mva_test/regulators/buck15_set /"$TARGET"_test/regulators/ldo7_set
fi

if [[ "$TARGET" = bd71847 ]] || [[ "$TARGET" == bd71850 ]]
then
	ln -sf /sys/kernel/mva_test/regulators/buck1_en /"$TARGET"_test/regulators/buck1_en
	ln -sf /sys/kernel/mva_test/regulators/buck1_set /"$TARGET"_test/regulators/buck1_set
	ln -sf /sys/kernel/mva_test/regulators/buck2_en /"$TARGET"_test/regulators/buck2_en
	ln -sf /sys/kernel/mva_test/regulators/buck2_set /"$TARGET"_test/regulators/buck2_set
	ln -sf /sys/kernel/mva_test/regulators/buck3_en /"$TARGET"_test/regulators/buck3_en
	ln -sf /sys/kernel/mva_test/regulators/buck3_set /"$TARGET"_test/regulators/buck3_set
	ln -sf /sys/kernel/mva_test/regulators/buck4_en /"$TARGET"_test/regulators/buck4_en
	ln -sf /sys/kernel/mva_test/regulators/buck4_set /"$TARGET"_test/regulators/buck4_set
	ln -sf /sys/kernel/mva_test/regulators/buck5_en /"$TARGET"_test/regulators/buck5_en
	ln -sf /sys/kernel/mva_test/regulators/buck5_set /"$TARGET"_test/regulators/buck5_set
	ln -sf /sys/kernel/mva_test/regulators/buck6_en /"$TARGET"_test/regulators/buck6_en
	ln -sf /sys/kernel/mva_test/regulators/buck6_set /"$TARGET"_test/regulators/buck6_set


	ln -sf /sys/kernel/mva_test/regulators/buck7_en /"$TARGET"_test/regulators/ldo1_en
	ln -sf /sys/kernel/mva_test/regulators/buck7_set /"$TARGET"_test/regulators/ldo1_set
	ln -sf /sys/kernel/mva_test/regulators/buck8_en /"$TARGET"_test/regulators/ldo2_en
	ln -sf /sys/kernel/mva_test/regulators/buck8_set /"$TARGET"_test/regulators/ldo2_set
	ln -sf /sys/kernel/mva_test/regulators/buck9_en /"$TARGET"_test/regulators/ldo3_en
	ln -sf /sys/kernel/mva_test/regulators/buck9_set /"$TARGET"_test/regulators/ldo3_set
	ln -sf /sys/kernel/mva_test/regulators/buck10_en /"$TARGET"_test/regulators/ldo4_en
	ln -sf /sys/kernel/mva_test/regulators/buck10_set /"$TARGET"_test/regulators/ldo4_set
	ln -sf /sys/kernel/mva_test/regulators/buck11_en /"$TARGET"_test/regulators/ldo5_en
	ln -sf /sys/kernel/mva_test/regulators/buck11_set /"$TARGET"_test/regulators/ldo5_set
	ln -sf /sys/kernel/mva_test/regulators/buck12_en /"$TARGET"_test/regulators/ldo6_en
	ln -sf /sys/kernel/mva_test/regulators/buck12_set /"$TARGET"_test/regulators/ldo6_set
fi

#Give regulator core some time to disable unused regulators which are not set as always-on
sleep 4
