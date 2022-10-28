# ROHM BD99954 Charger IC Linux device drivers.

The device driver for BD99954 can be found from the Linux mainline kernel.
Driver was first added in Linux v5.8-rc1.

For backporting you should also backport the linear_ranges code as well as
new battery bindings. We can also do backporting for you - please contact
Mr. Mikko Mutanen (mikko.mutanen AT fi.rohmeurope.com) for schedule and prizing
clarifications.

## ACPI
The community driver (as writing of this) does not support ACPI. There is an
early version of this driver with ACPI support - but it has not been
actively maintained. We can provide it to you but it might be safer
(and cheaper) to add ACPI support to the community kernel.

Again, if you need ACPI and don't feel comfortable adding ACPI support yourself
you please contact Mr. Mikko Mutanen (mikko.mutanen AT fi.rohmeurope.com) to
discuss the possibility of us integrating the driver to your system.

Linux kernel can be obtained from:

```
https://www.kernel.org/
```

or by cloning Linus Torvald's official linux development tree from:

```
git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux.git
```

Configuration options may want to enable are:
* CONFIG_CHARGER_BD70528 for power supply
