# ROHM Power Management IC BD96801 Linux device drivers.

The ROHM BD96801 "Scalable PMIC" is an automotive grade PMIC which can scale to different applications by allowing chaining of PMICs. BD96801 provides 4 BUCK regulators and 3 LDOs with configurable start-up sequences, voltages, and safety limits. A watchdog is also included.
For companion PMIC used together with BD96801 see [BD96802](../BD96802)

## Linux:

### Upstream:

An [RFC Series](https://lore.kernel.org/all/cover.1712920132.git.mazziesaccount@gmail.com/) has been sent to the upstream Linux kernel community and is being further reworked based on feedback from the community. The goal is to include the support for the BD96801 in the community Linux kernel during the first half of the 2024.

### Downstream drivers:

Currently there are two initial reference design releases. Please note that these drivers are intended to be used as a reference design only. No warranty is given and feasibility for target setup must be verified. Drivers are also not fully tested and bugs may be hiding. Evaluate at your own risk.

#### Simple driver:

This patch series brings initial and hopefully easy to understand support for configuring BUCK voltages and feeding the watchdog. All configurations which require the PMIC to be in STANDBY mode are unsupported. This includes the safety-limits. The INTB interrupts from the pre-programmed limits are to be handled though. ERRB interrupts are ignored by the driver because thet are likely to lead the SoC reset. Also the watchdog driver is included.

The version 001 is tagged in our Linux git tree as tag [bd96801-simple-v001](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/releases/tag/bd96801-simple-v001).

This driver has support for
* Regulators
* Watchdog

#### Driver with experimental features

This patch series brings initial and experimental support for
configuring voltages and safety limits on Linux system. Also a driver
for watchdog feeding is included.

The most notable limitation of the linux driver is the inability to
control the PMIC state. Most of the configurations can only be done when
the BD96801 is in STBY state, and this driver fails to do those
configurations when the PMIC is in any other state. These configurations
include:
 - over/under voltage/current/temperature detection/protection limits
 - Initial voltages ("INT" voltage in data-sheet. BUCKs have the "tune"
   range which can be configured also at ACTIVE state)
 - LDO voltages (LDOs only have INT voltage config)

If the control support for STANDBY-only configurations provided by this
driver are needed, then it is best to implement the STBY-line handling in
this driver to ensure the PMIC is in STBY state when the driver is
starting-up. This requires the 'ALWAYS_ON' registers to be preprogrammed
so that power-rails required by the processor running Linux and this driver
are enabled when PMIC is in STBY. Ensuring this configuration and adding
the STBY-line handling are not implemented in this driver. User converting
this reference driver to a real production driver must take care of it.

The driver supports the basic control of BUCKs and LDOs and configuring
Over/Under-voltage, over-current, over-temperature protections via
device-tree. Following constrains are worth noting:
* The voltages can be enabled/disabled and protections configured only
when the PMIC is in STANDBY state.
* The protections can't be disabled. UVP limit can't be configured.
OVP and OCP limits can be configured.
* OVD and UVD for a regulator have common limit.
* OCP and thermal protections are implemented by turning all INTB
notifications fatal. This means that OVD/UVD can not be used together
with OCP and thermal protection.
* Thermal limits can't be configured. TSD is fixed to 175 Celsius, and
thermal warning is fixed close to 140 Celsius.
* LDO's do not have own temperature monitor. The LDO limits use PMIC
core temperature.

The version 001 is tagged in our Linux git tree as tag [bd96801-experimental-v001](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/releases/tag/bd96801-experimental-v001).

This driver has support for
* Regulators
* Watchdog

### Known issues

When working with upstream Linux, please report bugs using the information found from the MAINTAINERS file located in the kernel sources. ROHM can't control what fixes or features get in the upstream Linux but we are actively following the Linux development mail lists and some of us are also listed in the MAINTAINER information.

For the downstream reference drivers, please see the bug-tracker for bd96801 Linux drivers related [known issues](https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/issues?q=is%3Aissue+repo%3ALinux-Kernel-PMIC-Drivers+BD96801+in%3Atitle). If you encounter a bug which is not known - feel free to report it - thanks!
