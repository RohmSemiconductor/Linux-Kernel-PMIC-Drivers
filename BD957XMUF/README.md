# ROHM Power Management IC BD9576MUF and BD9573MUF device drivers.

## Linux:

Device driver for BD9576MUF and BD9573MUF has been submitted to the Linux community kernel.
The community reviewing process is still ongoing for MFD and Watchdog portions.

Intermediate driver release can be found from [this branch](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/commits/bd9576-rohm)
Please see the commits on top of tag Linux 5.9-rc4.

This page will be updated when drivers are included in the community Linux kernel.

Configuration options may want to enable are:
* CONFIG_MFD_ROHM_BD957XMUF for BD9576/BD9573 core
* CONFIG_REGULATOR_BD957XMUF for regulator support
* CONFIG_BD957XMUF_WATCHDOG for watchdog support

