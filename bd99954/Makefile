#obj-m += bd71828-test.o bd71828-gpio-test.o bd71828-clktest.o

all: dtb
#bd71828-test: bd71828-test.c bd71828-gpio-test.c
#	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) modules
dtb: bd99954_test.dts
	dtc -O dtb -o bd99954_test.dtbo -b 0 -@ bd99954_test.dts

clean:
	rm -rf bd99954_test.dtbo
install:
	cp bd99954_test.dtbo ${CFG_BBB_NFS_ROOT}/.
#	cp bd71828-test.ko /home/mvaittin/nfs/.
#	cp bd71828-gpio-test.ko /home/mvaittin/nfs/.
#	cp bd71828-clktest.ko /home/mvaittin/nfs/.

#debug_cfg:
#	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) CONFIG_DEBUG_SECTION_MISMATCH=y modules
