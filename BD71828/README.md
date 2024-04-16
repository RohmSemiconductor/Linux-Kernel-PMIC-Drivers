# ROHM Power Management IC BD71828 / BD71878 / BD71879 device driver.

## Linux:

### Community drivers:

Device driver for BD71828/BD71878/BD71879 has been included in the Linux community kernel v5.6.

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

### Extensions - not fully tested

Here you can find additional patches for regulator run-level and LED control as well as for charger/fuel-gauge support. Please see the branch [stable-v5.4.6](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/stable-v5.4.6). Keep in mind that some GPIO/regulator features have been backported to this stable kernel and no full testing has been performed - you should be prepared to do testing and fixing as needed. Please treat these changes as a implementation reference only.

Configuration options may want to enable are:

```
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
