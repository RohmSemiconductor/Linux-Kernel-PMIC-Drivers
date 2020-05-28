# ROHM Power Management IC BD71828 / BD71878 device driver.

## Linux:

Device driver for BD71828/BD71878 has been included in the Linux community kernel v5.6.

<a href="https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/mfd/rohm-bd71828.c">BD71828 MFD driver</a>

Additional patches for regulator run-level and LED control as well as for charger/fuel-gauge support have been created but are not in upstream. Please let us know if you are interested in getting them. You can send us an email to matti.vaittinen@fi.rohmeurope.com (patches will be added here as well)

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
* CONFIG_MFD_ROHM_BD71828 for BD71828 core
* CONFIG_REGULATOR_BD71828 for regulator control
* CONFIG_COMMON_CLK_BD718XX for clock gate control
* CONFIG_GPIO_BD71828 for GPIO control
* CONFIG_RTC_DRV_BD70528 for RTC control

Configuration options for out of tree drivers:
* CONFIG_POWER_BD71828
```

The linux dt-documentations

```
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/mfd/rohm,bd71828-pmic.yaml
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/regulator/rohm,bd71828-regulator.yaml
```
