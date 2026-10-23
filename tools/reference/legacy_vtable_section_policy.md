# Historical vtable section policy

The prior scanner treated only .text as code with explicit section metadata.
Its fallback inferred executable/data ranges from reference-image section names
and rejected every target below 0x10000. Generic scanning now uses executable
flags, or known function bounds when flags are absent. Missing metadata cannot
safely discover previously unknown thunk ranges.

## Original fallback implementations

```python
def _get_code_ranges():
    """Get all code section VA ranges from config."""
    ranges = [(config.TEXT_VA_START, config.TEXT_VA_END)]

    # Add any other code sections defined in config
    if hasattr(config, 'SECTIONS'):
        skip_names = {".data", ".data1", "XIPS", "EnglishXlate", "JapaneseXlate",
                      "GermanXlate", "FrenchXlate", "SpanishXlate", "ItalianXlate"}
        for name, va, size, _ in config.SECTIONS:
            if name not in skip_names and name != ".text":
                ranges.append((va, va + size))

    return ranges


def _get_data_sections():
    """Get all data sections to scan for vtables."""
    sections = []

    # Primary scan range: .rdata or .data
    if hasattr(config, 'RDATA_RAW_ADDR'):
        sections.append({
            "name": "rdata",
            "va": config.RDATA_VA_START,
            "size": config.RDATA_VA_SIZE,
            "raw": config.RDATA_RAW_ADDR,
        })

    # Also scan all data sections from SECTIONS list
    if hasattr(config, 'SECTIONS'):
        data_names = {".data", ".data1", "DOLBY"}
        for name, va, size, raw in config.SECTIONS:
            if name in data_names:
                sections.append({
                    "name": name,
                    "va": va,
                    "size": size,
                    "raw": raw,
                })

    return sections

```
