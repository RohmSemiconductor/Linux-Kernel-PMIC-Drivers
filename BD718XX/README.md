# ROHM Power Management IC BD71837 and BD71847 device drivers.

## Linux:

Device driver for BD71837 and BD71847 can be found from the Linux community kernel.

The driver has been included in the Linux kernel since the Linux version
4.19-rc1. Please note that few crucial patches has been applied since then.
It is highly recommended to use driver included in the Linux kernel version
5.0-rc1 or later. On i.MX8 setups it is suggested to use Linux 5.1-rc1 or later because
the i.MX8 SNVS state support for BD71837 and BD71847 was included
in mainline kernel at release 5.1-rc1.

Linux kernel can be obtained from:

```
https://www.kernel.org/
```

or by cloning Linus Torvald's official linux development tree from:

```
git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux.git
```



Configuration options may want to enable are:
CONFIG_MFD_ROHM_BD718XX for BD71837/BD71847 core
CONFIG_REGULATOR_BD718XX for regulator control
CONFIG_COMMON_CLK_BD718XX for clock gate control
CONFIG_KEYBOARD_GPIO for reset induced by short press of power button.

## Das u-Boot:

The experimental u-Boot driver for BD71837 and BD71847 is sent to u-Boot mail list.
Patches can be found from


Cover-letter email:
```
https://lists.denx.de/pipermail/u-boot/2019-April/364314.html
```
Patches:
```
https://patchwork.ozlabs.org/patch/1080860/
https://patchwork.ozlabs.org/patch/1080863/
```
