# ROHM BD99954 Charger IC Linux device drivers.

The device driver for BD99954 is sent to Linux community and is - as
writing of this - present in power-supply tree for-next branch (with
some corrections in regulators tree). The driver should be merged to
mainline Linux v5.8.

## ACPI
Community driver (as writing of this) does not support ACPI. There is an
early version of this driver with ACPI support - but it has not been
actively maintained. We can provide it to you but it might be safer
(and cheaper) to add ACPI support to community kernel.

If you need ACPI and don't feel comfortable adding ACPI support yourself you
can contact Mr. Koki Okada (koki.okada(at)fi.rohmeurope.com) to discuss the
possibility of us integrating the driver to your system.

Power-supply Linux development tree:
```
https://git.kernel.org/pub/scm/linux/kernel/git/sre/linux-power-supply.git/
```

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
