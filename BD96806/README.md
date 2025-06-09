---
permalink: /BD96806/
patchlink: https://lore.kernel.org/all/cover.1744090658.git.mazziesaccount@gmail.com/
upstreamed: v6.16-rc1
configs:
  - config: CONFIG_MFD_ROHM_BD96801
    subsystem: mfd
  - config: CONFIG_REGULATOR_BD96801
    subsystem: regulator
  - config: CONFIG_BD96801_WATCHDOG
    subsystem: watchdog
---
# ROHM Power Management IC BD96806 Linux device drivers.

The ROHM BD96806 "Scalable PMIC" is an automotive grade PMIC which can scale to different applications by allowing chaining of PMICs. BD96806 provides 2 BUCK regulators with configurable start-up sequences, voltages, and safety limits. A watchdog is also included.
The BD96806 can be used as a companion PMIC for [ROHM BD96801](../BD96801) or [ROHM BD96805](../BD96805).

The ROHM BD96806 is almost identical to the [BD96802](../BD96802/). The main difference is different tuning voltage ranges.

## Linux:

{% include source_upstream_status.md %}

{% include kernel_conf_tbl.md %}

