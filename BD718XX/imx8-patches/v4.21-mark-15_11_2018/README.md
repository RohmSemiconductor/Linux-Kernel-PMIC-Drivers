# ROHM bd718x7 PMIC. i.MX8 specific set of patches

This patch set allows ROHM bd718x7 driver to work according to the i.MX8
requirements. The main benefit is that patch series allow PMIC to
transition to SNVS state when PMIC_ON_REQ or a power-button are used to
suspend the system. The SNVS state on the i.MX8 is a power saving state
where only the SNVS domain stays powered. Using this state with
BD71837/BD71847 is not straightforward because when SNVS is reached after
the poweroff state, the regulators with the SW control bit enabled will
stay unpowered. Thus if any boot critical regulator has the SW control
enabled the system fails to boot if SNVS is used. The community driver
emphasizes giving full control over regulators to the SW and always
transitions to READY state instead of SNVS. This is not optimal with i.MX8
because in READY state also SNVS domain is unpowered.

### content by patches:

**Patch 1**

First patch in series disables the SW enable/disable control from all the
other regulators except buck3 and buck4 - which are not boot critical on
the i.MX8 systems (typically connected to GPU and VPU) and which may provide
remarkable power savings when disabled.

**Patch 2**

The second patch changes the transition target to be SNVS instead of READY
for resets (suspends) caused by the PMIC_ON_REQ or power button presses.
Please note that this change is only required for kernel version 4.21
and onwards as it deals with a change that will be merged from Mark's tree
in official linux only during 4.21 merge window.

**Patch 3**

The third patch allows user to specify the HW run level (RUN, IDLE, SUSPEND)
specific voltages for DVS bucks.

**UPDATE: December 04th: Patch 4**

Patch 4 was created as a bugfix. It fixes the default DVS values introduced
in patch 3.

This patch series was created on top of Mark Brown's regulator-next tree
on November 15th 2018.

Patches 1, 3 and 4 were tested to apply on linux 4.20-rc1. Patch 2 is only
required from linux 4.21 and onwards. Please contact
matti.vaittinen@fi.rohmeurope.com if you have problems with the patches.


The patches are offered free of charge, "AS IS" without warranty.

```
---

Matti Vaittinen (4):
  regulator: bd718x7: Disallow SW control for critical regulators
  regulator: bd718x7: Go to SNVS instead of READY
  regulator: bd718x7: Support setting DVS buck HW state voltages
  regulator: bd718x7: Fix the buck1 - 4 default DVS voltages

 .../devicetree/bindings/mfd/rohm,bd71837-pmic.txt  |  18 +-
 drivers/regulator/bd718x7-regulator.c              | 478 +++++++++------------
 2 files changed, 217 insertions(+), 279 deletions(-)

-- 
2.14.3
```

