### Source Code:

{% if page.upstreamed %}

#### Upstream

    {% if page.upstreamlink %}
The ROHM Finland SWDC has created a Linux driver (or added support to an exsiting driver) in collaboration with the Linux kernel community. Driver is included in the [Upstream Linux]({{ page.upstreamlink }}) from {{ page.upstreamed }} onwards.
    {% else %}
The ROHM Finland SWDC has created a Linux driver (or added support to an exsiting driver) in collaboration with the Linux kernel community. Driver is included in the Upstream Linux from {{ page.upstreamed }} onwards.

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

    {% endif %}
{% else %}
    {% if page.patchlink %}

#### Upstream

Being upstreamed. See [Patches]({{ page.patchlink }}).
    {% endif %}
    {%- if page.expectupstreamed %}
#### Upstream plan

Upstream driver is currently expected to land in Linux {{ page.expectupstreamed }}
    {%- endif %}

{%- endif %}
{%- if page.downstreamlink %}

#### Downstream

Here is an unmaintained [reference driver]({{ page.downstreamlink }}) which you can try.
    {% if page.downstreamunstable %}
The driver branch is **unstable**, meaning it is **under development** and the git branch can be changed without a warning. Changes include rebases and other operations which can break the change history and modify the commits. If git tags are created, they should stay stable. Please note, drivers under development are expected to be buggy, sometimes even up to the point where compilation may fail. You should know the risks of trying such code. **Even hardware breakages are possible and it is user's responsibility to ensure the code is safe before using it**.
    {% endif -%}
Please note that this reference driver is provided as is, without a warranty. It is not a "production ready quality", and you are required to do all porting, fixing and testing while writing your driver using it as a starting point.
{%- endif %}

