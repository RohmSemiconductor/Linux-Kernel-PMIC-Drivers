# ROHM Power Management IC BD71815 Linux device drivers.

### 01/2021 ROHM mainstream driver for BD71815

We decided that it would be beneficial for all if these drivers were available
in the mainstream Linux community kernel. After few weeks of work, initial
[set of patches](https://lore.kernel.org/lkml/cover.1611037866.git.matti.vaittinen@fi.rohmeurope.com/T/#t)
was sent to the Linux kernel community in order to collect some feedback and
to initiate the driver's (long) journey to community kernel :) Please note that
the driver sent to upstream kernel does not yet contain the power-supply portion
because the ROHM power-supply driver contains a very IC specific fuel-gauge algorithm
and this may not fit as such to generic Linux driver. More work is required to
separate the fuel-gauge computations from IC specific code. We keep working on this
but can not guarantee the end result yet.

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

