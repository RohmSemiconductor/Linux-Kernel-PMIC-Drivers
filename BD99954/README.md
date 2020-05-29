# ROHM BD99954 Charger IC Linux device drivers.

The device driver for BD99954 has been sent to the Linux community and is - as
writing of this - present in power-supply tree's "for-next" branch (with
some corrections in the regulators tree). The driver should be merged to the
mainline Linux v5.8.

## ACPI
The community driver (as writing of this) does not support ACPI. There is an
early version of this driver with ACPI support - but it has not been
actively maintained. We can provide it to you but it might be safer
(and cheaper) to add ACPI support to the community kernel.

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
