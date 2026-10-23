# Historical kernel build summary

The prior kernel CMake header claimed:

```text
Replaces 147 Xbox kernel imports with Win32 equivalents.
```

This was a static implementation snapshot, not proof that every listed import
was complete or Win32-only. Current generic documentation describes host
implementations and guest ABI bridges without certifying import completeness.
CMake commands and build options are unchanged.
