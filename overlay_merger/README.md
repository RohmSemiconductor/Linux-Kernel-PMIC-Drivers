#Device-tree overlay driver for adding device tree blob(s) from user space to linux DT at runtime

#NOTE: This is deprecated. I suggest you to see the overlay adding mechanism maintained by Geert in Renesas tree.

Here we have a kernel module and build.sh && Makefile for compiling the module
You need kernel source which corresponds to kernel in your BBB. You should see linux_kernel_compilation folder for documentation on how to build the kernel. Additionally you need the cross compiler (see bb-compiler).

1. Edit build.sh so that kernel source directory matches your setup.
2. type Make
3. transfer mva_overlay.ko kernelmodule to BBB and insmod it.
4. see that /sys/kernel/mva_overlay/mva_overlay_add gets created
5. do: dd if=your_dt_overlay.dtbo of=/sys/kernel/mva_overlay/mva_overlay_add bs=1M count=1
6. Your device-tree should now be part of linux DT. See /proc/device-tree

__If you see error "no symbols in root of device tree" it is because your linux device-tree (not the overlay one but the one in system) was not compiled with -@ flag (overlay support)__ I solved this problem by compiling the linux device-tree with -@ option myself. In order to do that:
1. go in your linux source directory where you have compiled kernel.
2. go to folder arch/arm/boot/dts
3. compile the already preprocessed .am335x-boneblack.dtb.dts.tmp using command
    ```
    dtc -@ -I dts -O dtb -o yournewdtbfile.dtb .am335x-boneblack.dtb.dts.tmp
    ```
4. ignore warnings and copy your compiled yournewdtbfile.dtb to tftp folder and name it to something u-boot is configured to load (In my examples it is /var/lib/tftpboot/am335x-boneblack.dtb). back up original am335x-boneblack.dtb (just in case...)

