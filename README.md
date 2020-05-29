# Linux-Kernel-PMIC-Drivers
Rohm power management IC drivers for Linux kernel and u-Boot.

ROHM has been collaborating with the Linux kernel community and sees
the value of open-source. Hence we want to contribute to community
and attempt to upstream the Linux drivers for our power components.

## Upstream driver questions
If you have questions related to the Linux community drivers - please
use the linux community mail-lists and maintainer information. Once the
drivers are upstreamed the code changes are no longer in our hands - and the
best experts for those drivers can be found from the commnity. This does not
mean ROHM is out of the game - we have our personnel in Linux driver reviewers/
maintainers - but we don't "own" these components or frameworks anymore. You
get the best possible contacts via the MAINTAINERS file.

## Contents of this repository
Yet we occasionally develop something which does not perfectly fit
to upstream Linux frameworks or policies. It may be something which
is too product specific or something which requires functionality
not present in upstream kernels.

This is the place to look for ROHM POWER IC specific Linux driver extensions.
Please be aware that these extensions are provided as reference implementation
only and they are not actively developed/maintained.

## Linux driver integration to your system
We know our drivers. We know our HW. We can speed-up your development.
Interested? Feel free to contact Mr. Koki Okada (koki.okada(at)fi.rohmeurope.com)
to discuss about co-operation possibilities and project scheduling/prizing/scoping.

* Not upstreamed - [BD71815AGW PMIC](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD71815)
* ROHM extensions available - [BD71828 / BD71878 PMICs](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD71828)
* Fully upstreamed - [BD71837 / BD71847 / BD71850 PMICs](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD718XX)
* Fully upstreamed - [BD70528 PMIC](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD70528)
* ACPI not upstreamed BD99954 CHARGER - partially tested implementation to be added here (you can ask from matti.vaittinen(at)fi.rohmeurope.com untill added)
