# ROHM Power Management IC BD70528 Linux device drivers.

The device driver for BD70528 is included in the Linux community kernel.

The complete set of drivers for BD70528 was first included in Linux 5.3-rc1. 

Please note that the driver is intended to be used in use-cases where
processor running linux is directly connected to PMIC via i2c and PMIC
is controlled by the Linux core. Use case where M4 is used as power
manager is not supported by this driver. On such setups You want M4
with specific firmware (implementing PMIC control and RPMSG channel
towards Linux) - if PMIC control is required from Linux. Thus, for the
basic i.MX7ULP cases you probably have better support on NXP BSP - and
from linux side the NXP's pfuze1550-rpmsg driver should work nicely or
require only minor modifications.

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

