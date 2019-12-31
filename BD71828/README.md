# ROHM Power Management IC BD71828 device driver.

## Linux:

Device driver for BD71828 has been partially submitted to the Linux community kernel.

Patch series for basic SW support of BD71828 can be found from <a href="https://lore.kernel.org/lkml/cover.1576054779.git.matti.vaittinen@fi.rohmeurope.com/">Patch series v6</a>
Please note, there is also reworked patch series (currently at v8) but it does not contain all the patches as part of them were already applied to upstream kernel trees. This page will be updated with kernel version containing all the patches when they are all merged to mainline kernel.

Additional patches for run-level control and charger/fuel-gauge support have been created but are not in upstream. Please let us know if you are interested in getting them. You can send us an email to matti.vaittinen@fi.rohmeurope.com

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
* CONFIG_LEDS_BD71828 for LED control
* CONFIG_RTC_DRV_BD70528 for RTC control

Configuration options for out of tree drivers:
* CONFIG_POWER_BD71828
```

The linux dt-documentations

```
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/mfd/rohm,bd71828-pmic.yaml
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/regulator/rohm,bd71828-regulator.yaml
```
