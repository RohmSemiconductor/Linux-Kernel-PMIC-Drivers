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

The 32.768 Hz clock gate driver is not yet in the official linux tree. 
You can get the support for controlling the clock using the [common clock framework](https://www.kernel.org/doc/Documentation/clk.txt)
by applying patches 1,2,3 and 10 from [this](https://lore.kernel.org/linux-clk/20181204114527.GC31204@localhost.localdomain/T/#t) patch series.

There is also additional patches for BD71837 with i.MX8. Please see:
https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD718XX/imx8-patches

Configuration options you need to enable are:
CONFIG_MFD_ROHM_BD718XX and CONFIG_REGULATOR_BD718XX (and CONFIG_COMMON_CLK_BD718XX if you apply the extra patches for clock support)
