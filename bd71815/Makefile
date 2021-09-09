obj-m += bd71815-test.o bd71815-gpio-test.o bd71815-clktest.o

all: bd71815-test dtb
bd71815-test: bd71815-test.c bd71815-gpio-test.c bd71815-clktest.c
	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) modules
dtb: bd71815_test.dts
	dtc -O dtb -o bd71815_test.dtbo -b 0 -@ bd71815_test.dts

clean:
	rm -rf bd71815_test.dtbo bd71815-clktest.ko
install:
	cp bd71815_test.dtbo /home/mvaittin/nfs/.
	cp bd71815-test.ko /home/mvaittin/nfs/.
	cp bd71815-gpio-test.ko /home/mvaittin/nfs/.
	cp bd71815-clktest.ko /home/mvaittin/nfs/.

#debug_cfg:
#	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) CONFIG_DEBUG_SECTION_MISMATCH=y modules
