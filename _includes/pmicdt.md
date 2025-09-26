## The device-tree

The PMIC drivers (as well as the drivers consuming regulator-output, GPIOs or other resources provided by the PMIC) will require information about the hardware from the device-tree. The device-tree is a hardware description which must be written to describe the connections and devices in the board where the PMIC is placed. In some cases the device-tree also needs to describe the used PMIC's OTP configuration so drivers know what features are provided by the PMIC. Correctly written device-tree is mandatory for the drivers to work.

Device-tree binding documents describe the properties that can be added to the device-tree. These documents are written in YAML format, so that the actual device-tree can be verified using scripts/tools.

{% if page.bindings -%}
#### The PMIC specific bindings:
    {%- for b in page.bindings %}
* [{{b.linktext}}]({{b.link}})
    {%- endfor %}

The bindings are provided for documentory purposes only, "as is" and with no warranty.
{%- endif -%}
