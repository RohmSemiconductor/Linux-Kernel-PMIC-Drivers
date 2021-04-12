# ROHM Power Management IC BD9576MUF and BD9573MUF device drivers.

## Linux:

Device driver for BD9576MUF and BD9573MUF has been submitted to the Linux community kernel.
Regulator driver has been in upstream kernel since Linux v5.10.

MFD and watchdog drivers are expected to land in Linux v5.13. They are introduced in MFD tree at this [branch](https://git.kernel.org/pub/scm/linux/kernel/git/lee/mfd.git/log/?h=ib-mfd-watchdog-5.13).

Please note that the regulator driver name was modified during MFD review process and the rgulator driver name must be changed to match name used in MFD as is done in this [patch](https://lore.kernel.org/lkml/9fd467d447cd2e002fa218a065cd0674614b435f.1615454845.git.matti.vaittinen@fi.rohmeurope.com/). 

### Safety limits and warnings
The BD9576MUF was designed to support notifying SoC from various problem conditions before problems are so severe they damage the hardware.
The PMIC allows configuring per regulator notification limits for over/under voltages and over current. Additionally there are protection
limits when PMIC will shut down regulator outputs to protect the hardware from further damages.

ROHM has in co-operation with the Linux kernel community developed Linux regulator framework extension so that these warnings can be fully utilized.
This extension was first sent as an RFC patch series for further reviewing and improving. The RFC has now stabilized and evolved into proper patch
series. You can find warning support and BD9576MUF driver which
emits these notifications [from patch v6](https://lore.kernel.org/lkml/cover.1617789229.git.matti.vaittinen@fi.rohmeurope.com/)

This page will be updated when drivers are included in the community Linux kernel.

Configuration options may want to enable are:
* CONFIG_MFD_ROHM_BD957XMUF for BD9576/BD9573 core
* CONFIG_REGULATOR_BD957XMUF for regulator support
* CONFIG_BD957XMUF_WATCHDOG for watchdog support

