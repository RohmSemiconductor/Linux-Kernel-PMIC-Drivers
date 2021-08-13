# ROHM Power Management IC BD71815 Linux device drivers.

### 01/2021 ROHM mainstream driver for BD71815

First set of the BD71815 PMIC drivers were included in the mainstream Linux
version 5.13-rc1. Please note that the driver sent to upstream kernel does not
yet contain the power-supply portion because the ROHM power-supply driver
contains a very IC specific fuel-gauge algorithm and this may not fit as such
to a generic Linux driver. More work is required to separate the fuel-gauge
computations from IC specific code. We keep working on this but can not
guarantee the end result yet. If you have a project where you need the
battery fuel-gauge implemented in BD71815 driver you may:

1. Get the upstream driver and add a power-supply driver in it.
2. Get the whole old reference driver below and test + fix it.
3. Get the upstream driver + SW-gauge draft and test + fix it.

I would definitely go with option 1 or 3. The upstream driver is likely to be receiving
testing and bug-fixes by others. It is also likely to be ported on new kernel versions.
The old reference driver linked to bottom of the page is not maintained.

- [upstream kernel](https://www.kernel.org)
- [swgauge RFC (OUTDATED, see unofficial development version below) (no BD71815)](https://lore.kernel.org/lkml/cover.1607085199.git.matti.vaittinen@fi.rohmeurope.com/)
- [swgauge + BD71815 unofficial development version](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/swgauge-on-5.13) BEWARE: UNSTABLE, MAY BE REBASED!

### Reference driver ported on Linux v.4.9.99

A Linux driver for the ROHM BD71815 Power Management IC is available
[here](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/v4.9.99-BD71815AGW).
Please note that this driver has been originally written for an early Linux 4.9
kernel and has not been actively maintained. Here we have a port to the more
recent Linux v.4.9.99 - but this port has not been fully tested. Please treat
this as a reference design only. See also the issues found from this version:
https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/issues?q=is%3Aissue+is%3Aclosed+BD71815
These issues are fixed in upstream linux patches referred above.

Please find the driver ported on Linux v4.9.99 [here](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/v4.9.99-BD71815AGW)

This driver has support for
* MFD
* GPIO
* power-supply
* regulators
* RTC
