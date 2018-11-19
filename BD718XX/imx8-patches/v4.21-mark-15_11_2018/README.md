# ROHM bd718x7 PMIC. i.MX8 specific set of patches

This patch set allows ROHM bd718x7 driver to work according to i.MX8
requirements. The main benefit is that patch series allows PMIC to
transition to SNVS state when PMIC_ON_REQ or power-button are used to
suspend the system. SNVS state on i.MX8 is power saving state where only
SNVS domain stays powered. Using this state with BD71837/BD71847 is not
straightforward because when SNVS is reached after poweroff state, the
regulators with SW control bit enabled will stay unpowered. Thus if any
boot critical regulator has SW control enabled the system fails to boot
if SNVS is used. Community driver emphasizes giving full control to
regulators to SW and always transitions to READY state instead of SNVS.
This is not optimal with i.MX8 because in READY state also SNVS domain
is unpowered.

First patch in series disables SW enable/disable control from all other
regulators except buck3 and buck4 - which are not boot critical on i.MX8
systems (typically connected to GPU and VPU) and which may provide
remarkable power savings when disabled.

Second patch changes the transition target to be SNVS instead of READY
for resets (suspends) caused by PMIC_ON_REQ or power button presses.

Last patch allows user to specify HW run level (RUN, IDLE, SUSPEND)
specific voltages for DVS bucks.

This patch series was created on top of Mark Brown's regulator-next tree
at NOV 15 2018. Latest released official linux kernel is version 4.20
.
Patches 1 and 3 were tested to apply on linux 4.20-rc1. Patch 2 is only
required from linux 4.21 and onwards. Please contact
matti.vaittinen@fi.rohmeurope.com if you have problems with the patches.

---

Matti Vaittinen (3):
  regulator: bd718x7: Disallow SW control for critical regulators
  regulator: bd718x7: Go to SNVS instead of READY
  regulator: bd718x7: Support setting DVS buck HW state voltages

 .../devicetree/bindings/mfd/rohm,bd71837-pmic.txt  |  18 +-
 drivers/regulator/bd718x7-regulator.c              | 478 +++++++++------------
 2 files changed, 217 insertions(+), 279 deletions(-)

-- 
2.14.3

