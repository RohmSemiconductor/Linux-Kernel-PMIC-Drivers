---
permalink: /BD957XMUF/
upstreamed: v5.13
configs:
  - config: CONFIG_MFD_ROHM_BD957XMUF
    subsystem: mfd
  - config: CONFIG_REGULATOR_BD957XMUF
    subsystem: regulator
  - config: CONFIG_BD957XMUF_WATCHDOG
    subsystem: watchdog
---
# ROHM Power Management IC BD9576MUF and BD9573MUF.

## Linux Driver:

{% include source_upstream_status.md %}

(The regulator name in MFD driver cell was changed to "bd9576-regulator" and "bd9573-regulator". If your regulator driver is not probed or the module is not load, please ensure you have matching platform_device_id in drivers/regulator/bd9576-regulator.c. The old version had name set to "bd9573-pmic".)

The name was fixed in Linux release v5.14-rc1.

### Safety limits and warnings

The BD9576MUF was designed to support notifying SoC from various problem conditions before problems are so severe they damage the hardware.
The PMIC allows configuring per regulator notification limits for over/under voltages and over current. Additionally there are protection
limits when PMIC will shut down regulator outputs to protect the hardware from further damages.

ROHM has in co-operation with the Linux kernel community developed Linux regulator framework extension so that these warnings can be fully utilized.
This extension is expeted to be included in the Linux release v5.14. See the regulator subsystem [pull-request](https://lore.kernel.org/lkml/20210628145501.EC10F60C3E@mail.kernel.org/) for more information. You can specify board specific safety limits via device-tree and
implement own handlers the regulator core notifiers will call to cope with the safety warnings/errors prior hardware originated forced shutdown.

{% include kernel_conf_tbl.md %}

{% include upstream_support.md %}
