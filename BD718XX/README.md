# ROHM Power Management IC BD71837 and BD71847 Linux device driver.

Device driver for BD71837 and BD71847 can be found from the Linux community kernel.

The driver has been included in the Linux kernel since the Linux version
4.19-rc1. Please note that few crucial patches has been applied since then.
It is highly recommended to use driver included in the Linux kernel version
5.0-rc1 or later.

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

There is also additional patches for BD71837 / BD71847 with i.MX8. Please see:
https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD718XX/imx8-patches
Main difference between community driver and i.MX8 specific driver is
that on i.MX8 patch set we support keeping i.MX8 SNVS domain powered
after resets at a cost of disallowing enable/disable control for most
of the regulators.

The i.MX8 SNVS state support for BD71837 and BD71847 will be included
in mainline kernel at release 5.1. The change is already available in the
linux-next and Mark Brown's regulator trees.

Configuration options may want to enable are:
CONFIG_MFD_ROHM_BD718XX for BD71837/BD71847 core
CONFIG_REGULATOR_BD718XX for regulator control
CONFIG_COMMON_CLK_BD718XX for clock gate control
CONFIG_KEYBOARD_GPIO for reset induced by short press of power button.
