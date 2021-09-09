obj-m += bd71847-test.o
obj-m += bd71847-test2.o

all: dtb
	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) modules
dtb: bd71847_test.dts
	dtc -O dtb -o bd71847_test.dtbo -b 0 -@ bd71847_test.dts

clean:
	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) clean

install:
	cp bd71847-test.ko /home/mvaittin/nfs/.
	cp bd71847-test2.ko /home/mvaittin/nfs/.
	cp bd71847_test.dtbo /home/mvaittin/nfs/.

debug_cfg:
	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) CONFIG_DEBUG_SECTION_MISMATCH=y modules
