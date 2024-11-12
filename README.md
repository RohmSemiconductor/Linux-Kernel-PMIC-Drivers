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

## Upstream driver testing
A few automated tests are being ran for the upstreamed ROHM PMIC drivers. The
tests are ran for tags created from the:
- Torvald's main Linux repository
- Linux-Next integration testing repository
- Linux stable repository
The test results are upload to the [PMIC branch](https://github.com/RohmSemiconductor/rohm-linux-test-results/tree/PMIC) of the rohm-linux-test-results repository.

## Contents of this repository
We occasionally develop something which does not perfectly fit into
the upstream Linux frameworks or policies. This content may include something
which is too product specific or something which requires functionality not
present in upstream kernels.

This is the place to look for ROHM POWER IC specific Linux driver extensions.
Please be aware that these extensions are provided as reference implementation
only and they are not actively developed/maintained.

### Upstream status
* ROHM extensions available - [BD71815AGW PMIC](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD71815)
* ROHM extensions available - [BD71828 / BD71878 PMICs](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD71828)
* Fully upstreamed - [BD71837 / BD71847 / BD71850 PMICs](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD718XX)
* Fully upstreamed - [BD9576 / BD9573](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD957XMUF)
* Upstreamed - no ACPI [BD99954 CHARGER](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD99954)
* Not upstream - [BD2657](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD2657)
* Partially upstreamed - [BD96801 "Scalable PMIC"](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD96801)
* Obsoleted (Fully upstreamed, removed) - [BD70528 PMIC](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD70528)

### Generic upstream effort
ROHM aims giving back to the community. We know it's two-way road, really. We get
from the community a working platform, bug fixes, porting to new versions,
discussions, education and a chance to impact the direction Linux is heading
to. We value all of this and want to give back innovations and improvements
which may be small or big and aren't always directly relaed to our products.
This helps us all. Here are some things we have participated and are working
on - maybe you find something that is helpful to you too.

[Generic Linux improvements](generic-linux-improvements/)
