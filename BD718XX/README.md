# ROHM Power Management IC BD71837, BD71847 and BD71850 device drivers.

## Linux:

Device driver for BD71837, BD71847 and BD71850 can be found from the Linux community kernel.

The driver has been included in the Linux kernel since the Linux version
4.19-rc1. Please note that few crucial patches has been applied since then.
It is highly recommended to use driver included in the Linux kernel version
5.0-rc1 or later. On i.MX8 setups it is suggested to use Linux 5.1-rc1 or later because
the i.MX8 SNVS state support for BD71837 and BD71847 was included
in mainline kernel at release 5.1-rc1. Finally, support for leaving enable/disable
states of given regulators under HW-state machine seems to be landing in 5.10. (It is
available in Linux-Next integration tree as writing of this)

Please not that there is no own device-tree compatible in old Linux releases for the
BD71850. If such Linux release is used the BD71850 can be
described using exactly same device-tree bindings as BD71847. Please use

compatible = "rohm,bd71847";

also for BD71850 if

compatible = "rohm,bd71850";

is not recognized.

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
* CONFIG_MFD_ROHM_BD718XX for BD71837/BD71847 core
* CONFIG_REGULATOR_BD718XX for regulator control
* CONFIG_COMMON_CLK_BD718XX for clock gate control
* CONFIG_KEYBOARD_GPIO for reset induced by short press of power button.

## Das u-Boot:

Limited u-Boot regulator driver for BD71837 and BD71847 is included in the official Denx u-boot. First u-boot release containing the driver is the 1.st release candidate for 2019.10 (version u-boot-2019.10-rc1). The u-boot driver works with pmic device-tree which is compatible with dt-documentation included in the Linux source code but a few of the properties are ignored. BD71850 can be used with same u-boot driver using the BD71847 device-tree bindings.

The Denx u-boot:

```
https://www.denx.de/wiki/U-Boot/SourceCode
ftp://ftp.denx.de/pub/u-boot/
```

The linux dt-documentations

```
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/mfd/rohm,bd71837-pmic.txt
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/regulator/rohm,bd71837-regulator.txt
```
