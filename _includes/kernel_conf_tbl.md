### Configuration:

Configuration options may want to enable for kernel build are:


```
{% for c in page.configs -%}
- {{ c.config }}
{% endfor -%}
```

| config | description | subsystem |
---------|-------------|-----------|
{% for c in page.configs -%}
{%- if c.description -%}
    {%- capture desc -%} {{ c.description }} {%- endcapture -%}
{%- else -%}
    {%- capture ifile -%} defaulttxt/{{- c.subsystem -}}-cfg-desc.md {%- endcapture -%}
    {%- capture desc -%} {%- include {{ ifile }} -%} {%- endcapture -%}
{%- endif -%}
| {{ c.config }} | {{ desc | strip_newlines }} | {{ c.subsystem }} |
{% endfor -%}


