# ROHM Power Management IC BD2657 device driver.

## Linux:

Device driver for BD2657 is being worked on.

The ROHM BD2657 Power Management IC is intended to be used for systems
powered by one Li-Ion or Li-Ion polymer battery cell, or by an input from
2.85 V to 5.5 V. The device provides 4 buck converters supporting dynamic
voltage scaling. Programmable output voltage, sequencing and power state
control support a wide variety of processors and system implementations.
The BD2657 also provides 2 general purpose outputs and a power button support.

You can check the [unofficial repository for the development](https://github.com/M-Vaittinen/linux/tree/bd2657-dev).
Please note the branches in this repository are rebased/moved/edited without a warning.

```
* CONFIG_MFD_ROHM_BD2657 for BD2657 core
* CONFIG_REGULATOR_BD2657 for regulator control
* CONFIG_GPIO_BD2657 for GPIO control
```

The linux dt-documentations

```
TBD
```
