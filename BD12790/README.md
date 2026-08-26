---
permalink: /BD12790/
bindings:
  - link: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/hwmon/adi,adm1275.yaml
    linktext: BD12790 (and other similar ICs) binding documentation
configs:
  - config: CONFIG_SENSORS_ADM1275
    subsystem: PMBus hwmon
    description: enables driver supporting PMBus based hardware monitoring

upstreamed: v7.2
---

# ROHM BD12790 Hot-Swap controller Linux device drivers.

The ROHM BD12790 is a Hot-Swap controller with PMBus Interface.
It has load current, input voltage, output voltage, external
N-channel FET power and temperature monitors via integrated
12-bit ADC.

The BD12790 controls the gate voltage of the
external N-channel FET so that the load current is
maintained around the current limit level. The controller
sets the current profile and the amount of power that can
be supplied by the FET, keeping the FET within the safe
operating area. In the event of a short circuit, the internal
overcurrent detector initiates shut down. Afterwards, the
gate recovers control within 50μs, minimizing
interruptions in conditions such as line steps or surges.

The BD12790MUV has under voltage and over voltage
detection of the input voltage at UVH, UVL and OV pins,
and the detection voltage levels can be programmed with
external resistor divider. The output voltage is also
monitored at PWGIN pin with external resistor divider
and the PWRGD signal is the indicator if the input and
output voltages are within normal range.

## The device-tree

{% include driverdt.md %}

## Linux:

{% include source_upstream_status.md %}

{% include kernel_conf_tbl.md %}
