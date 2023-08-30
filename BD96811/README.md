# ROHM Power Management IC BD96811 Linux device drivers.

The ROHM BD96811 is another of ROHM's "Scalable PMICs". It is an automotive grade PMIC which can scale to different applications by allowing chaining of PMICs. BD96811 provides 5 voltage outputs which can be configured to different type of regulators depending on the OTP setup. The regulators have onfigurable voltages and safety limits. A watchdog is also included. The BD96811 can be used as a companion PMIC for [ROHM BD96801](../BD96801)

## Linux:

Currently there is a initial reference driver for Linux available tagged in our Linux git tree as tag [scalable-bd96811-unstable-alpha-v0.01](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/releases/tag/scalable-bd96811-unstable-alpha-v0.01).
Please note that the driver is intended to be used as a reference design only. No warranty is given and feasibility for target setup must be verified. Driver is also not fully tested and bugs may be hiding. Evaluate at your own risk.

The driver implements following features:

Initial and experimental support for configuring voltages and safety limits on Linux system. Also a driver for watchdog feeding is included.

The most notable limitation of this version of linux driver is the lack of IRQ support.

A special care must be taken when writing the device-tree for BD96811 due to the
high OTP configurability. The driver requires information about the OTP used
on chip. This information must be correctly provided via device-tree. Please pay
careful attention to the DT bindings.

The driver supports the basic control of regulators and configuring
Over/Under-voltage protections via device-tree. Following constrains are worth
noting:

Limitations of this SW version:
- Regulator error Notifications or flags are not provided.
- IRQs are not supported.
- OCP and TW limit setting is not implemented.

Limitations coming from HW design:

- The protections can't be disabled.
- configurable OVP / UVP limits are implemented by turning all detection IRQs
  fatal. This means that OVD/UVD limits can not be used when any of the OVP/UVP
   limit is configured using fatal notifications.
- OVD and UVD for a regulator have common limit setting, Eg, setting OVD limit
  changes also UVD limit. Driver checks for configuration mismatches only when
  limit for both OVP and UVP is set using the fatal OVD / UVD.

This driver has support for
* Regulators
* Watchdog

Bugs from this version can be reported in GitHub [issue tracker](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/issues) or to matti.vaittinen@fi.rohmeurope.com
