### Source Code:

{% if page.upstreamed %}
    {%- if page.upstreamlink -%}
The ROHM Finland SWDC has created a Linux driver in collaboration with the Linux kernel community. Driver is included in the [Upstream Linux]({{ page.upstreamlink }}) from {{ page.upstreamed }} onwards.
    {%- else -%}
The ROHM Finland SWDC has created a Linux driver in collaboration with the Linux kernel community. Driver is included in the Upstream Linux from {{ page.upstreamed }} onwards.

The Linux kernel can be obtained from:

```
https://www.kernel.org/
```

or by cloning Linus Torvald's official linux development tree from:

```
git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux.git
```

    {%- endif -%}

{%- else %}
    {%- if page.patchlink %}
Being upstreamed. See [Patches]({{ page.patchlink }}).
    {%- endif %}
    {%- if page.downstreamlink %}
Here is an unmaintained [reference driver]({{ page.downstreamlink }}) which you can try. Please note that this reference driver is provided as is, without a warranty. It is not a "production ready quality", and you are required to do all porting, fixing and testing while writing your driver using it as a starting point.
    {%- endif %}
    {%- if page.expectupstreamed %}

Upstream driver is currently expected to land in Linux {{ page.expectupstreamed }}
    {%- endif %}
{%- endif %}


