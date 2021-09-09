#obj-m := bd71837.o
#bd71837-y := bd71837-regulator.o mfd-bd71837.o clk-bd71837.o
#obj-m := mfd-bd71837.o
#obj-m += clk-bd71837.o
#obj-m += bd71837-regulator.o
obj-m += bd71837-test.o

all: dtb
	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) modules
dtb: bd71837_test.dts
	dtc -O dtb -o bd71837_test.dtbo -b 0 -@ bd71837_test.dts
	dtc -O dtb -o bbb_only_I2C_1.dtbo -b 0 -@  bbb_only_I2C_1.dts

clean:
	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) clean

install:
	cp bd71837-test.ko /home/mvaittin/nfs/.
	cp bd71837_test.dtbo /home/mvaittin/nfs/.
	cp bbb_only_I2C_1.dtbo /home/mvaittin/nfs/.

debug_cfg:
	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) CONFIG_DEBUG_SECTION_MISMATCH=y modules
