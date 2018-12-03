# ROHM bd718x7 PMIC. i.MX8 specific set of patches

This patch set backports the ROHM BD718x7 PMIC driver on top of
the Linux stable kernel version 4.9.136. (Patches 1-19 + patch 22)

Patches 20, 21 and 27 change the community driver to work for powering
NXP i.MX8 processor as explained in data-sheet. This means following
changes compared to a plain community driver:

The main benefit is that the patch series allow PMIC to transition to SNVS
state when PMIC_ON_REQ or a power-button are used to suspend the system.
The SNVS state on the i.MX8 is a power saving state where only the SNVS domain
stays powered. Using this state with BD71837/BD71847 is not straightforward
because when SNVS is reached after the power-off state, the regulators with
the SW control bit enabled will stay unpowered. Thus if any boot critical
regulator has the SW control enabled the system fails to boot if SNVS is used.
The community driver emphasizes giving full control over regulators to the SW
and thus always transitions to READY state instead of SNVS. This is not optimal
with i.MX8 because in READY state also SNVS domain is unpowered.

Patch 0020 disables the SW enable/disable control from all other regulators
except buck3 and buck4 - which are not boot critical on i.MX8
systems (typically connected to GPU and VPU) and which may provide
remarkable power savings when disabled.

Patch 0021 allows user to specify the HW run level (RUN, IDLE, SUSPEND)
specific voltages for DVS bucks.

Patch 0027 fixes format of default HW run level voltages which are used
when the voltages are not given from device-tree.

This patch series was created on top of linux-stabe tree tag 2.9.136
on December 03rd 2018.

Please contact matti.vaittinen@fi.rohmeurope.com if you have problems with
the patches.

Patches are offered free of charge, "AS IS" without warranty.

```
---

Axel Lin (3):
  regulator: bd71837: Simplify bd71837_set_voltage_sel_restricted
    implementation
  regulator: bd71837: Remove duplicate assignment for n_voltages of LDO2
  regulator: bd718x7: Remove struct bd718xx_pmic

Geert Uytterhoeven (1):
  regulator: bd718x7: Remove double indirection for
    bd718xx_pmic_inits.rdatas

Matti Vaittinen (23):
  regulator: bd71837: Devicetree bindings for BD71837 regulators
  regulator: bd71837: BD71837 PMIC regulator driver
  regulator: bd71837: Editorial cleanups.
  regulator: bd71837: Remove duplicate description from DT bindings
  Input: gpio_keys - add missing include to gpio_keys.h
  regulator: bd71837: adobt MFD changes to regulator driver
  mfd: bd71837: Core driver for ROHM BD71837 PMIC
  mfd: bd71837: Devicetree bindings for ROHM BD71837 PMIC
  regulator: regmap helpers - support overlapping linear ranges
  regulator: bd71837: Disable voltage monitoring for LDO3/4
  regulator: bd718x7: add missing linux/of.h inclusion
  regulator/mfd: Support ROHM BD71847 power management IC
  regulator: dt bindings: add BD71847 device-tree binding documentation
  mfd: dt bindings: add BD71847 device-tree binding documentation
  regulator: Support regulators where voltage ranges are selectable
  regulator/mfd: bd718xx: rename bd71837/bd71847 common instances
  regulator: bd718XX use pickable ranges
  regulator: bd718xx: rename bd71837 to 718xx
  regulator: bd718xx: fix build warning on x86_64
  regulator: bd718x7: Disallow SW control for critical regulators
  regulator: bd718x7: Support setting DVS buck HW state voltages
  regulator: bd718x7: add i2c_id
  regulator: bd718x7: Fix the buck1 - 4 default DVS voltages

 .../devicetree/bindings/mfd/rohm,bd71837-pmic.txt  |   79 ++
 .../bindings/regulator/rohm,bd71837-regulator.txt  |  124 +++
 drivers/mfd/Kconfig                                |   13 +
 drivers/mfd/Makefile                               |    1 +
 drivers/mfd/rohm-bd718x7.c                         |  207 ++++
 drivers/regulator/Kconfig                          |   11 +
 drivers/regulator/Makefile                         |    1 +
 drivers/regulator/bd718x7-regulator.c              | 1065 ++++++++++++++++++++
 drivers/regulator/core.c                           |    5 +
 drivers/regulator/helpers.c                        |  243 ++++-
 include/linux/gpio_keys.h                          |    2 +
 include/linux/mfd/rohm-bd718x7.h                   |  338 +++++++
 include/linux/regulator/driver.h                   |   20 +-
 13 files changed, 2103 insertions(+), 6 deletions(-)
 create mode 100644 Documentation/devicetree/bindings/mfd/rohm,bd71837-pmic.txt
 create mode 100644 Documentation/devicetree/bindings/regulator/rohm,bd71837-regulator.txt
 create mode 100644 drivers/mfd/rohm-bd718x7.c
 create mode 100644 drivers/regulator/bd718x7-regulator.c
 create mode 100644 include/linux/mfd/rohm-bd718x7.h

-- 
2.14.3


``
