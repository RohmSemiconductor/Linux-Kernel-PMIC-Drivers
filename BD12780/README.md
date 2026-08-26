---
permalink: /BD12780/
bindings:
  - link: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/hwmon/adi,adm1275.yaml
    linktext: BD12780 (and other similar ICs) binding documentation
configs:
  - config: CONFIG_SENSORS_ADM1275
    subsystem: PMBus hwmon
    description: enables driver supporting PMBus based hardware monitoring

upstreamed: v7.2
---

# ROHM BD12780 Hot-Swap controller Linux device drivers.

The ROHM BD12780 is a Hot-Swap controller with PMBus Interface.
It has load current, input voltage, output voltage, external
N-channel FET power and temperature monitors via integrated
12-bit ADC.

The BD12780 controls the gate voltage of the
external N-channel FET so that the load current is
maintained around the current limit level. When the load
current reaches the current limit threshold, the timer
operates to limit the load current with the FET ON for the
time determined by the capacitor connected to the
TIMER pin. In addition, a constant power foldback
scheme is used to control MOSFET power consumption
in the event of a power-on or failure. This power limit
keeps the FET within the safe operating area. When a
short-circuit event occurs, the fast internal overcurrent
detector responds within 320 ns and shuts down the gate
of the external FET.

## The device-tree

{% include driverdt.md %}

## Linux:

{% include source_upstream_status.md %}

{% include kernel_conf_tbl.md %}
