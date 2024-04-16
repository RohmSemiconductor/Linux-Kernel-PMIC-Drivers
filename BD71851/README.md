# ROHM Power Management IC BD71851

The ROHM BD71851 is a Power Management IC which has:
8 BUCK regulators and 4 LDOs. There's also an ADC with an accumulator for
current/voltage sensing, RTC and 32.768 kHz clock gate and interrupt capable
GPIOs*.

(*)Available functions depend on OTP configuration. The pins are shared and
amount of available GPIOs depend on what are the other enabled functions

## Linux:

Linux drivers are being worked on.

## uBoot:

An uBoot driver for evaluating some of the PMIC functionalities is done. Please
contact matti.vaittinen(at)fi.rohmeurope.com for the details.
