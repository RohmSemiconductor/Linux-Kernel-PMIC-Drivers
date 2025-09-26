---
permalink: /BD72720/
configs:
  - config: CONFIG_MFD_ROHM_BD71828
    subsystem: mfd
  - config: CONFIG_REGULATOR_BD71828
    subsystem: regulator
  - config: CONFIG_COMMON_CLK_BD718XX
    subsystem: clk
  - config: CONFIG_GPIO_BD72720
    subsystem: gpio
  - config: CONFIG_RTC_DRV_BD70528
    subsystem: rtc
  - config: CONFIG_KEYBOARD_GPIO
    subsystem: input
    description: Enables the gpio-keys driver for power-button events.
  - config: CONFIG_BD71828_HALL
    subsystem: misc
    description: Enables a driver for sending LID events from HALL sensor.
  - config: CONFIG_LEDS_BD71828
    subsystem: led
  - config: CONFIG_CHARGER_BD71828
    subsystem: power-supply
downstreamlink: https://github.com/RohmSemiconductor/Linux-Kernel-PMIC-Drivers/tree/bd72720-reference-driver-v1
downstreamunstable: true
---

# ROHM Power Management IC BD72720

The ROHM BD72720 is a Power Management IC which can be controlled using I2C. Features integrated in the PMIC include:
10 BUCK regulators and 11 LDOs.
3000mA single-cell switching charger.
An ADC with an accumulator for current/voltage sensing (and a coulomb counter).
RTC and 32.768 kHz clock gate
Power control inputs for power-button and HALL sensor.
Interrupt capable GPIOs*.


(*)Available functions depend on OTP configuration. The pins are shared and
amount of available GPIOs depend on what are the other enabled functions

## Linux:

{% include source_upstream_status.md %}

{% include kernel_conf_tbl.md %}

