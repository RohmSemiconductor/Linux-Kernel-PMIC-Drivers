# ROHM bd718x7 PMIC. i.MX8 specific set of patches

This patch set allows ROHM bd718x7 driver to work according to i.MX8
requirements. The main benefit is that the patch series allows PMIC to
transition to SNVS state when PMIC_ON_REQ or a power-button are used to
suspend the system. The SNVS state on the i.MX8 is a power saving state
where only the SNVS domain stays powered. Using this state with
BD71837/BD71847 is not straightforward because when SNVS is reached after
power-off state, the regulators with the SW control bit enabled will stay
unpowered. Thus if any boot critical regulator has the SW control enabled
the system fails to boot if SNVS is used. The community driver emphasizes
giving full control over regulators to SW and thus always transitions to
READY state instead of SNVS. This is not optimal with i.MX8 because in READY
state also the SNVS domain is unpowered.

The patches also allow the user to specify the HW run level
(RUN, IDLE, SUSPEND) specific voltages for DVS bucks.

Currently patches are tested on the kernel 4.20-rc1 and 4.9.136. Also some
changes coming to the kernel 4.21 are considered in patch set for 4.20.
See the details for [linux 4.20](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD718XX/imx8-patches/v4.21-mark-15_11_2018/) and [linux 4.9](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/master/BD718XX/imx8-patches/v4.9.136-stable-03_12_2018/)

### tar.gz archived BD718x7 I.MX8 patches:
* [for linux 4.20](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/raw/master/BD718XX/imx8-patches/linux-bd718x7-v4.21-mark-15_11_2018-updated_04_12.tar.gz)
* [for linux 4.9.136](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/raw/master/BD718XX/imx8-patches/linux-bd718x7-v4.9.136-stable-03_12_2018.tar.gz)

Please contact matti.vaittinen@fi.rohmeurope.com if you have problems with the
patches.

The patches are offered free of charge, "AS IS" without warranty.
