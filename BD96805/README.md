---
permalink: /BD96805/
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
# ROHM Power Management IC BD96805 Linux device drivers.

The ROHM BD96805 "Scalable PMIC" is an automotive grade PMIC which can scale to different applications by allowing chaining of PMICs. BD96805 provides 4 BUCK regulators and 3 LDOs with configurable start-up sequences, voltages, and safety limits. A watchdog is also included.
For companion PMIC used together with BD96805 see [BD96802](../BD96802) and [BD96806](../BD96806).

The ROHM BD96805 is almost identical to the [BD96801](../BD96801/). The main difference is different tuning voltage ranges.

## Linux:

{% include source_upstream_status.md %}

{% include kernel_conf_tbl.md %}

