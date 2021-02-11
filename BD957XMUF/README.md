# ROHM Power Management IC BD9576MUF and BD9573MUF device drivers.

## Linux:

Device driver for BD9576MUF and BD9573MUF has been submitted to the Linux community kernel.
The community reviewing process is still ongoing for MFD and Watchdog portions.

Latst set of patches are [version 8](https://lore.kernel.org/lkml/cover.1613031055.git.matti.vaittinen@fi.rohmeurope.com/)

### Safety limits and warnings
The BD9576MUF was designed to support notifying SoC from various problem conditions before problems are so severe they damage the hardware.
The PMIC allows configuring per regulator notification limits for over/under voltages and over current. Additionally there are protection
limits when PMIC will shut down regulator outputs to protect the hardware from further damages.

ROHM has in co-operation with the Linux kernel community developed Linux regulator framework extension so that these warnings can be fully utilized.
This extension is currently sent as an RFC patch series for further reviewing and improving. You can find warning support and BD9576MUF driver which
emits these notifications [from here](https://lore.kernel.org/lkml/cover.1613042245.git.matti.vaittinen@fi.rohmeurope.com/)

This page will be updated when drivers are included in the community Linux kernel.

Configuration options may want to enable are:
* CONFIG_MFD_ROHM_BD957XMUF for BD9576/BD9573 core
* CONFIG_REGULATOR_BD957XMUF for regulator support
* CONFIG_BD957XMUF_WATCHDOG for watchdog support

