# Linux-Kernel-PMIC-Drivers
Rohm power management IC drivers for Linux kernel and u-Boot.

ROHM has been collaborating with the Linux kernel community 
to upstream the Linux drivers for some of our power components.

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

## The Linux-Kernel-PMIC-Drivers repository
We occasionally develop something which does not perfectly fit into
the upstream Linux frameworks or policies. This content may include something
which is too product specific or something which requires functionality not
present in upstream kernels.

The [Linux-Kernel-PMIC-Drivers](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers) repository is the place to look for ROHM POWER IC specific Linux driver extensions.
Please be aware that these extensions are provided as reference implementation
only and they are not actively developed/maintained.

### Upstream driver status
* [BD71815AGW PMIC](BD71815) - ROHM extensions available
* [BD71828 / BD71878 PMICs](BD71828) - ROHM extensions available
* [BD71879 PMICs](BD71879) - ROHM extensions available
* [BD71837 / BD71847 / BD71850 PMICs](BD718XX) - Fully upstreamed
* [BD9576 / BD9573](BD957XMUF) - Fully upstreamed
* [BD99954 CHARGER](BD99954) - Fully upstreamed (no ACPI support)
* [BD96801 "Scalable PMIC"](BD96801) - ROHM extensions available
* Obsoleted (Fully upstreamed, removed) - [BD70528 PMIC](BD70528)

### Not yet upstream
* [BD71851](BD71851)
* [BD96802](BD96802)
* [BD96811](BD96811)

### Not upstream
* [BD2657](BD2657)
* BD72720
* [BD71827](BD71827)

### Generic upstream effort
ROHM aims giving back to the community. We know it's two-way road, really. We get
from the community a working platform, bug fixes, porting to new versions,
discussions, education and a chance to impact the direction Linux is heading
to. We value all of this and want to give back innovations and improvements
which may be small or big and aren't always directly relaed to our products.
This helps us all. Here are some things we have participated and are working
on - maybe you find something that is helpful to you too.

[Generic Linux improvements](generic-linux-improvements/)
