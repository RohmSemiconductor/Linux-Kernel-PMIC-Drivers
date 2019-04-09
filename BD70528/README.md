# ROHM Power Management IC BD70528 Linux device drivers.

Device driver for BD70528 is submitted to the Linux community kernel.

The regulator patches are included in Linux 5.1-rc1. They do require
the MFD patches to be functional. Further features such as GPIO,
RTC, watchdog, battery charger and clock have own patches (see below).

Please note that the driver is intended to be used in use-cases where
processor running linux is directly connected to PMIC via i2c and PMIC
is controlled by the Linux core. Use case where M4 is used as power
manager is not supported by this driver. On such setups You want M4
with specific firmware (implementing PMIC control and RPMSG channel
towards Linux) - if PMIC control is required from Linux. Thus, for the
basic i.MX7ULP cases you probably have better support on NXP BSP - and
from linux side the NXP's pfuze1550-rpmsg driver should work nicely or
require only minor modifications.

BD70528 Linux driver patches (based on Linux 5.1-rc2):
* [explanatory cover-letter](https://lore.kernel.org/lkml/cover.1554371464.git.matti.vaittinen@fi.rohmeurope.com/)
* [preparatory header split](https://lkml.org/lkml/diff/2019/4/4/957/1)
* [MFD core](https://lkml.org/lkml/diff/2019/4/4/960/1)
* [clock](https://lkml.org/lkml/diff/2019/4/4/963/1)
* [dt-binding document](https://lkml.org/lkml/diff/2019/4/4/967/1)
* [GPIO](https://lkml.org/lkml/diff/2019/4/4/971/1)
* [RTC](https://lkml.org/lkml/diff/2019/4/4/974/1)
* [power supply](https://lkml.org/lkml/diff/2019/4/4/975/1)
* [watchdog](https://lkml.org/lkml/diff/2019/4/4/977/1)

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
* CONFIG_MFD_ROHM_BD70528 for BD70528 core
* CONFIG_REGULATOR_BD70528 for regulator control
* CONFIG_COMMON_CLK_BD718XX for clock gate control
* CONFIG_GPIO_BD70528 for GPIO
* CONFIG_RTC_DRV_BD70528 for RTC
* CONFIG_CHARGER_BD70528 for power supply
* CONFIG_BD70528_WATCHDOG for watchdog

