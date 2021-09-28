obj-m += mva_overlay.o


all:
	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) modules

clean:
	make -C ${KERNEL_DIR} CROSS_COMPILE=${CC} ARCH=arm M=$(PWD) clean
install:
	cp mva_overlay.ko ${CFG_BBB_NFS_ROOT}
