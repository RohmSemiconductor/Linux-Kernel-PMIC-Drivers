# ROHM Power Management IC BD71837 Linux device driver.

Device driver for BD71837 can be found from the Linux community kernel.

The driver has been included in the Linux kernel since the Linux version
4.19-rc1.
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

The 32.768 Hz clock gate driver is not yet in the official linux release. It is currently included in Linus Torvald's tree and will get included in linux release 4.21 if there is no surprizes. Meanwhile you can get the support for controlling the clock using the [common clock framework](https://www.kernel.org/doc/Documentation/clk.txt)
by applying these patches:
[clk: Add kerneldoc to managed of-provider interfaces](https://patchwork.kernel.org/patch/10711567/),
[clk: of-provider: look at parent if registered device has no provider info](https://patchwork.kernel.org/patch/10711571/),
[clk: bd718x7: Initial support for ROHM bd71837/bd71847 PMIC clock ](https://patchwork.kernel.org/patch/10717793/).

There is also additional patches for BD71837 with i.MX8. Please see:
https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD718XX/imx8-patches

Configuration options you need to enable are:
CONFIG_MFD_ROHM_BD718XX and CONFIG_REGULATOR_BD718XX (and CONFIG_COMMON_CLK_BD718XX if you apply the extra patches for clock support)
