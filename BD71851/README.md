---
permalink: /BD71851/
configs:
  - config: CONFIG_MFD_ROHM_BD71828
    subsystem: mfd
  - config: CONFIG_REGULATOR_BD71828
    subsystem: regulator
  - config: CONFIG_COMMON_CLK_BD718XX
    subsystem: clk
  - config: CONFIG_GPIO_BD71851
    subsystem: gpio
  - config: CONFIG_RTC_DRV_BD70528
    subsystem: rtc
downstreamlink: https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/bd71851-v1
patchlink: https://lore.kernel.org/all/cover.1785838584.git.mazziesaccount@gmail.com/
downstreamunstable: true
expectupstreamed: Linux v7.3
---

# ROHM Power Management IC BD71851 and BD73800

The ROHM BD71851 is a Power Management IC which has:
8 BUCK regulators and 4 LDOs. There's also an ADC with an accumulator for
current/voltage sensing, RTC and 32.768 kHz clock gate and interrupt capable
GPIOs*.

(*)Available functions depend on OTP configuration. The pins are shared and
amount of available GPIOs depend on what are the other enabled functions

The BD73800 is updated variant, supported by the same Linux software.

## Linux:

{% include source_upstream_status.md %}

{% include kernel_conf_tbl.md %}

## uBoot:

An uBoot driver for evaluating some of the PMIC functionalities is done. Please
contact matti.vaittinen(at)fi.rohmeurope.com for the details.
