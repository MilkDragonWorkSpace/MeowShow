#!/usr/bin/env python3
"""
auto_gif.py — One-click: meow.gif → Core/Inc/meow_anim.h

Usage:
    python tools/auto_gif.py

Generates the C header with properly composited GIF frames, uniformly
sampled to fit the STM32F103C8T6 64 KB Flash budget.
"""

import sys, os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
from gif_to_c import generate, FRAME_BYTES

PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..'))
GIF_PATH     = os.path.join(PROJECT_ROOT, 'meow1.gif')
OUTPUT_H     = os.path.join(PROJECT_ROOT, 'Core', 'Inc', 'meow_anim.h')
DEBUG_DIR    = os.path.join(SCRIPT_DIR, 'debug_frames')


def main():
    print("=" * 60)
    print("  MeowShow — GIF to OLED Animation Converter")
    print("=" * 60)
    print()

    if not os.path.exists(GIF_PATH):
        print(f"Error: meow.gif not found at {GIF_PATH}")
        sys.exit(1)

    try:
        from PIL import Image
    except ImportError:
        print("Error: Pillow is required. Install with: pip install pillow")
        sys.exit(1)

    # Generate with:
    #   - White canvas background (matches web-transparent GIFs)
    #   - Threshold 140 (good default for dark-on-light content)
    #   - Max 32 frames (32 KB for frames, 32 KB for code)
    #   - Debug output to tools/debug_frames/
    header = generate(
        gif_path=GIF_PATH,
        anim_name="meow",
        threshold=140,
        bg_color=(255, 255, 255),
        max_frames=32,
        debug_dir=DEBUG_DIR,
    )

    # Patch display name to "Meow!" while keeping C identifier as "meow"
    header = header.replace('.name        = "meow"',
                            '.name        = "Meow!"')

    # Write output
    os.makedirs(os.path.dirname(OUTPUT_H), exist_ok=True)
    with open(OUTPUT_H, 'w', encoding='utf-8') as f:
        f.write(header)

    # Count frames in output
    import re as _re
    frame_count = len(_re.findall(r'static const uint8_t \w+_frame_\d+', header))
    total_kb = frame_count * FRAME_BYTES / 1024.0
    code_est_kb = 14.0

    print()
    print(f"  Output:  {OUTPUT_H}")
    print(f"  Frames:  {frame_count}")
    print(f"  Data:    {total_kb:.1f} KB")
    print(f"  Code:    ~{code_est_kb:.0f} KB (estimated)")
    print(f"  Total:   ~{total_kb + code_est_kb:.0f} KB / 64 KB "
          f"({(total_kb + code_est_kb) / 64 * 100:.0f}%)")
    print()
    print(f"  Debug frames saved to: {DEBUG_DIR}/")
    print(f"  (oled_*.png = binarized preview of what the OLED will show)")
    print()
    print("  Done! Rebuild and re-flash the project.")
    print()
    print("  To adjust binarization, run gif_to_c.py directly:")
    print("    python tools/gif_to_c.py meow.gif meow --threshold 100 --debug-dir tools/debug_frames")


if __name__ == '__main__':
    main()
