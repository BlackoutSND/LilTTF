#!/usr/bin/env python3
"""
TrueType Font (.ttf) to C Source Array Converter for LilTTF.

Extracts 'maxp' table bounds (max points, max contours) using fonttools,
generates C byte array in Flash (.rodata) and header file with auto-calculated
buffer definitions.
"""

import argparse
import os
import re
import sys
from typing import Optional


def sanitize_identifier(name: str) -> str:
    """Sanitize a string to be a valid C identifier component."""
    return re.sub(r"[^a-zA-Z0-9_]", "_", name)


def convert_to_c_source(
    input_file: str,
    suffix: str = "",
    output_dir: Optional[str] = None
) -> bool:
    """
    Convert a TrueType font file to C source and header files.

    :param input_file: Path to the input .ttf font file.
    :param suffix: Optional suffix for generated file names and variable identifiers.
    :param output_dir: Directory where generated files will be written. Defaults to current working directory.
    :return: True if successful, False otherwise.
    """
    if not os.path.isfile(input_file):
        print(f"Error: File '{input_file}' not found.", file=sys.stderr)
        return False

    suffix = str(suffix) if suffix is not None else ""
    if suffix:
        clean_suffix = sanitize_identifier(suffix)
        if clean_suffix != suffix:
            print(f"Warning: Suffix '{suffix}' sanitized to '{clean_suffix}' for C identifiers.", file=sys.stderr)
            suffix = clean_suffix

    base_name = f"FontData{suffix}"
    output_file_name = f"{base_name}.c"
    header_file_name = f"{base_name}.h"
    array_name = base_name
    include_guard = f"FONT_DATA{suffix.upper()}_H" if suffix else "FONT_DATA_H"

    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
        output_file = os.path.join(output_dir, output_file_name)
        header_file = os.path.join(output_dir, header_file_name)
    else:
        output_file = output_file_name
        header_file = header_file_name

    try:
        from fontTools.ttLib import TTFont
    except ImportError:
        print(
            "Error: 'fontTools' library is not installed. Please install it using 'pip install fonttools'",
            file=sys.stderr
        )
        return False

    try:
        font = TTFont(input_file)
        if "maxp" not in font:
            print(f"Error: 'maxp' table missing from '{input_file}'.", file=sys.stderr)
            return False

        maxp = font["maxp"]
        max_points = int(getattr(maxp, "maxPoints", 0) or 0)
        max_contours = int(getattr(maxp, "maxContours", 0) or 0)
        max_composite_points = int(getattr(maxp, "maxCompositePoints", 0) or 0)
        max_composite_contours = int(getattr(maxp, "maxCompositeContours", 0) or 0)

        abs_max_points = max(max_points, max_composite_points)
        abs_max_contours = max(max_contours, max_composite_contours)

        print(f"Font Analysis for '{input_file}':")
        print(f"  Max Points (Simple):          {max_points}")
        print(f"  Max Contours (Simple):        {max_contours}")
        print(f"  Max Points (Composite):       {max_composite_points}")
        print(f"  Max Contours (Composite):     {max_composite_contours}")
        print(f"  Required MAX_POINTS_PER_GLYPH:   {abs_max_points}")
        print(f"  Required MAX_CONTOURS_PER_GLYPH: {abs_max_contours}")

    except Exception as e:
        print(f"Error extracting font metadata from '{input_file}': {e}", file=sys.stderr)
        return False

    try:
        with open(input_file, "rb") as f:
            data = f.read()
    except OSError as e:
        print(f"Error reading '{input_file}': {e}", file=sys.stderr)
        return False

    # Format C source array
    c_lines = [
        f'#include "{header_file_name}"\n\n',
        f"/* Auto-generated font binary array from: {os.path.basename(input_file)} */\n",
        f"/* Total size: {len(data)} bytes */\n",
        f"const uint8_t {array_name}[] = {{\n"
    ]
    for i in range(0, len(data), 12):
        chunk = data[i : i + 12]
        bytes_str = "".join(f"0x{b:02x}, " for b in chunk)
        c_lines.append(f"  {bytes_str}\n")
    c_lines.append("};\n\n")
    c_lines.append(f"const uint32_t {array_name}_SIZE = sizeof({array_name});\n")

    try:
        with open(output_file, "w") as f:
            f.writelines(c_lines)
    except OSError as e:
        print(f"Error writing '{output_file}': {e}", file=sys.stderr)
        return False

    header_content = f"""#ifndef {include_guard}
#define {include_guard}

#include <stdint.h>

#ifdef __cplusplus
extern "C" {{
#endif

/* Auto-generated memory limits based on {os.path.basename(input_file)} */
/* Max Points (Simple: {max_points}, Composite: {max_composite_points}) */
#if !defined(MAX_POINTS_PER_GLYPH) || (MAX_POINTS_PER_GLYPH < {abs_max_points})
  #undef MAX_POINTS_PER_GLYPH
  #define MAX_POINTS_PER_GLYPH {abs_max_points}
#endif

/* Max Contours (Simple: {max_contours}, Composite: {max_composite_contours}) */
#if !defined(MAX_CONTOURS_PER_GLYPH) || (MAX_CONTOURS_PER_GLYPH < {abs_max_contours})
  #undef MAX_CONTOURS_PER_GLYPH
  #define MAX_CONTOURS_PER_GLYPH {abs_max_contours}
#endif

extern const uint8_t {array_name}[];
extern const uint32_t {array_name}_SIZE;

#ifdef __cplusplus
}}
#endif

#endif /* {include_guard} */
"""

    try:
        with open(header_file, "w") as f:
            f.write(header_content)
    except OSError as e:
        print(f"Error writing '{header_file}': {e}", file=sys.stderr)
        return False

    print(f"Successfully generated:\n  - {output_file}\n  - {header_file}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert TrueType Font (.ttf) files to C byte arrays and headers with buffer limits."
    )
    parser.add_argument("input_ttf", help="Path to input TTF font file")
    parser.add_argument(
        "suffix",
        nargs="?",
        default="",
        help="Optional suffix for generated C files and variables (e.g. '1' -> FontData1.c/.h)"
    )
    parser.add_argument(
        "-o", "--output-dir",
        default=None,
        help="Optional output directory for generated C source and header files"
    )

    args = parser.parse_args()
    success = convert_to_c_source(args.input_ttf, args.suffix, args.output_dir)
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
