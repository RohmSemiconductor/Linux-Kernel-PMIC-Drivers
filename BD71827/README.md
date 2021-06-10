# ROHM Power Management IC BD71827

## Linux:

### This repository does currently not contain driver for BD71827.

The charger block of BD71827 is largely similar to the one used with
BD71815 and BD71828. The experimental BD71828/BD71815 charger driver
should be a good starting point and it should include untested support
for BD71827 charger. Feel free to contact Mr. Koki Okada
koki.okada@fi.rohmeurope.com for further details and possible co-opearation.

For experimental under-development version of ROHM BD71815/27/28/78 charger
driver please see [this untested, unstable, development branch](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/blob/swgauge-on-5.13/drivers/power/supply/bd71827-power.c).
Please keep in mind that this is highly unstable development branch which
may be rebased, reworked, in a state which has bugs or is not even compiling.
Yet you should get an idea how the BD71827 charger driver should look like.

