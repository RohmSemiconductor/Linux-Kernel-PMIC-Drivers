#ROHM Power Management IC BD71837 Linux device driver.

Device driver for BD71837 can be found from linux community kernel.
Now, August 23.rd 2018 the driver can be found from Linus Torwald's
official linux tree but is not yet included in any released kernel.
It is expected the driver will be first included in kernel version 4.19.

You can clone the Linux development tree containing kernel from:

git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux.git

Configuration options you need to enable are:
CONFIG_MFD_ROHM_BD718XX and CONFIG_REGULATOR_BD718XX

