#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

source $DIR/../../bb-compiler/setcc

function do_devtree() {
	local DTREENAME=$1

	if [[ $DTREENAME != "" ]]
	then
		DTREENAME=bbb-dt-"$DTREENAME"
		echo "devicetree name given as $DTREENAME"
	else
		DTREENAME=bbb-dt-$(date "+%Y-%m-%d-%s%N")
		echo "devicetree name not given. using $DTREENAME"
	fi
	cd arch/arm/boot/dts/
	dtc -@ -I dts -O dtb -o "$DTREENAME" .am335x-boneblack.dtb.dts.tmp
	if [ $? -eq 0 ]
	then
		echo "Main BBB Devicetree compiled"
	else
		echo "DTC compiler failed"
		exit -1
	fi

	cd -
	sudo mv "arch/arm/boot/dts/$DTREENAME" "/var/lib/tftpboot/$DTREENAME"
	sudo chown root:root "/var/lib/tftpboot/$DTREENAME"
	sudo chmod a+rx "/var/lib/tftpboot/$DTREENAME"
	cd /var/lib/tftpboot/
	sudo ln -sf "$DTREENAME" am335x-boneblack.dtb
#	sudo chmod a+rx am335x-boneblack.dtb
	cd -
}

function zimagoi() {
	local ZIMAGE=$1

	if [[ $ZIMAGE != "" ]]
	then
		ZIMAGE=zImage-"$ZIMAGE"
		echo "zImage name given as $ZIMAGE"
	else
		ZIMAGE=zImage-$(date "+%Y-%m-%d-%s%N")
		echo "zImage name not given. using $ZIMAGE"
	fi
	sudo cp arch/arm/boot/zImage "/var/lib/tftpboot/$ZIMAGE"
	cd /var/lib/tftpboot/
	sudo ln -sf "$ZIMAGE" zImage
	cd -
}

if [[ $1 == "help" ]]
then
	echo "Usage: ./paalaa.sh [help zImageonly compileonly zImage] [imagename]"
	exit 0
fi

if [[ $1 == "menuconfig" ]]
then
	make ARCH=arm CROSS_COMPILE=${CC} LOADADDR=0x80008000 menuconfig
	exit 0
fi

if [[ $1 == "dtcheck" ]]
then
	make ARCH=arm CROSS_COMPILE=${CC} LOADADDR=0x80008000 dt_binding_check
	exit 0
fi

if [[ $1 == "zImageonly" ]]
then
	zimagoi $2
	exit 0
fi

if [[ $1 == "dtree" ]]
then
	do_devtree $2
	if [ $? -eq 0 ]
	then
		exit 0
	else
		exit -1
	fi
fi

make -j8 olddefconfig ARCH=arm CROSS_COMPILE=${CC} LOADADDR=0x80008000
RES=$?
if [ $RES -eq 0 ]
then
	make -j8 ARCH=arm CROSS_COMPILE=${CC} LOADADDR=0x80008000
	RES=$?
	if [ $RES -eq 0 ]
	then
		if [[ $1 == "compileonly" ]]
		then
			exit 0
		fi

		sudo make ARCH=arm CROSS_COMPILE=${CC} LOADADDR=0x80008000 INSTALL_MOD_PATH=/home/mvaittin/nfs modules_install
		RES=$?
		if [ $RES -eq 0 ]
		then
			echo "Mod Install was success"
			if [[ $1 == "zImage" ]]
			then
				echo "zImage param found"
				zimagoi $2
			fi
		fi
	fi
fi
exit $RES

