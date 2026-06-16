Many of the device drivers require information about the hardware surrounding hardware. The device-tree is a hardware description, which must be written to describe the connections and ICs in the board. In some cases, the device-tree also needs to describe the used OTP configuration, so drivers know what features are provided by the IC they control. Correctly written device-tree is mandatory for many of the drivers to work.

Device-tree binding documents describe the properties that can be added to the device-tree. These documents are written in YAML format, so that the actual device-tree can be verified using scripts/tools.

{% if page.bindings -%}
#### This device's bindings:
    {%- for b in page.bindings %}
* [{{b.linktext}}]({{b.link}})
    {%- endfor %}

The bindings are provided for documentory purposes only, "as is" and with no warranty.
{%- endif -%}
