#!/usr/bin/env python3
"""Convert a PNG to a C++ header with embedded binary data."""
import sys, os

src = sys.argv[1] if len(sys.argv) > 1 else "Assets/knob_strip.png"
dst = sys.argv[2] if len(sys.argv) > 2 else "Source/KnobStripData.h"

with open(src, "rb") as f:
    data = f.read()

varname = os.path.basename(src).replace(".", "_").replace("-", "_")

with open(dst, "w") as f:
    f.write("// Auto-generated — do not edit. Run tools/png_to_header.py to regenerate.\n")
    f.write("#pragma once\n#include <cstddef>\n\n")
    f.write(f"static const unsigned char {varname}[] = {{\n")
    for i, b in enumerate(data):
        if i % 16 == 0:
            f.write("    ")
        f.write(f"0x{b:02x},")
        if i % 16 == 15:
            f.write("\n")
    f.write("\n};\n")
    f.write(f"static const size_t {varname}_size = {len(data)};\n")

print(f"Written {len(data):,} bytes → {dst}  (var: {varname})")
