# ROHM generic Linux improvements

ROHM aims to collaborate with the Linux kernel community to improve Linux kernel and drivers also outside the scope of ROHM component drivers. We want to give back to community but also want to stay on track regarding what happens in the Linux. This is a great way to improve our common software while also being able to impact on the direction it develops. This page lists some of the improvements ROHM employees are working on.

## Under development:

### Improve Linux's in-kernel battery fuel-gauge support

This is an attempt to add some fuel-gauge logic to power-supply core.

We try to add in-kernel entity performing iterative SOC estimation and coulomb counter correction for devices with a (drifting) coulomb
counter. This should allow few charger/fuel-gauge drivers to use generic loop instead of implementing their own.

You can check the [unofficial repository for the development](https://github.com/M-Vaittinen/linux/tree/simple-gauge-rfc4).
Please note the branches in this repository can be rebased or even moved/deleted without a warning.

Latest upstream patch series [RFC v3](https://lore.kernel.org/lkml/cover.1637061794.git.matti.vaittinen@fi.rohmeurope.com/#t)


## Examples of completed and upstreamed improvements:
### Linux regulator framework's protection extension

The extension adds following improvements:

1. WARNING level events/error flags.
	Current regulator 'ERROR' event notifications for over/under voltage, over current and over temperature are used to indicate
	condition where monitored entity is so badly "off" that it actually indicates a hardware error which can not be recovered. The most
	typical hanling for that is believed to be a (graceful) system-shutdown. Here we add set of 'WARNING' level flags to allow
	sending notifications to consumers before things are 'that badly off' so that consumer drivers can implement recovery-actions.

2. Device-tree properties for specifying limit values.
	Add limits for above mentioned 'ERROR' and 'WARNING' levels (which send notifications to consumers) and also for a 'PROTECTION' level
	(which will be used to immediately shut-down the regulator(s) W/O informing consumer drivers. Typically implemented by hardware).
	Property parsing is implemented in regulator core which then calls callback operations for limit setting from the IC drivers. A
	warning is emitted if protection is requested by device tree but the underlying IC does not support configuring requested protection.

3. Helpers which can be registered by IC.
	Target is to avoid implementing IRQ handling and IRQ storm protection in each IC driver. (Many of the ICs implementin these IRQs do not allow
	masking or acking the IRQ but keep the IRQ asserted for the whole duration of problem keeping the processor in IRQ handling loop).

4. Emergency poweroff function (refactored out of the thermal-core to kernel/reboot.c)
	 Function to be called if IC fires error IRQs but IC reading fails and given retry-count is exceeded. Improve the shutdown-funcionality so it is allowed to be called from any context.

5. Regulator device print helpers exported

### Regmap-IRQ main register support

Support IRQ controllers which have multiple "IRQ blocks" with status and mask registers, and one (or more) "main status"-register(s) indicating which "sub IRQ" blocks have active IRQs. This register setup can be usefull when IRQ registers are read over (relatively) slow bus and when only part of the IRQ blocks have frequently active IRQs. This design can help decreasing amount of register reads.

ROHM has extended the Linux general purpose "regmap-IRQ" IRQ contoller code to support the main-register setup.

### Links to few generic Linux improvements contributed by ROHM

- [Linux regulator framework's protection extension](https://lore.kernel.org/lkml/20210628145501.EC10F60C3E@mail.kernel.org/)
- [Regmap-IRQ main IRQ register support](https://lore.kernel.org/lkml/20190123175732.298F51127ABA@debutante.sirena.org.uk/)
- [Regmap-IRQ improve IRQ type configuration](https://lore.kernel.org/lkml/20181218115931.GA21253@localhost.localdomain/) and [fix to it](https://lore.kernel.org/lkml/20181227084443.GA23991@localhost.localdomain/)
- [Regulator framework, support pickable voltage ranges](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=18e4b55fbd2069cee51ef9660b35c65ec13bee6d)
- [linear-ranges helpers and their use in regulator framework](https://lore.kernel.org/lkml/20200601122156.GC45647@sirena.org.uk/)
- numerous small fixes and improvements which are not listed here

