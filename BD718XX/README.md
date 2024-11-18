---
permalink: /BD718XX/
upstreamed: 4.19-rc1
#items:
configs:
  - config: CONFIG_MFD_ROHM_BD718XX
    subsystem: mfd
  - config: CONFIG_REGULATOR_BD718XX
    subsystem: regulator
  - config: CONFIG_COMMON_CLK_BD718XX
    subsystem: clk
  - config: CONFIG_KEYBOARD_GPIO
    description: Enables support for sending shutdown request using power button.
    subsystem: input
---
# ROHM Power Management IC BD71837, BD71847 and BD71850 device drivers.

## Linux:

{% include source_upstream_status.md %}

NOTE:
Please note that a multiple crucial patches has been applied since initial release.
It is highly recommended to use driver included in the most recent Linux kernel version.

The device-tree compatible for the BD71850 is missing from initial releases. If such Linux release is used the BD71850 can be
described using exactly same device-tree bindings as BD71847. Please use

compatible = "rohm,bd71847";

also for BD71850 if

compatible = "rohm,bd71850";

is not recognized.

{% include kernel_conf_tbl.md %}

{% include upstream_support.md %}

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
