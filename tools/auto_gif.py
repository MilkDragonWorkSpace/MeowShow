#!/usr/bin/env python3
"""
auto_gif.py — One-click: GIF → Core/Inc/<name>_anim.h

Usage:
    python tools/auto_gif.py <gif_path> [options]
    python tools/auto_gif.py meow.gif
    python tools/auto_gif.py meow.gif -n mycat -t 120 -m 24
    python tools/auto_gif.py meow.gif -o Core/Inc/my_anim.h

Generates a C header with properly composited GIF frames, uniformly
sampled to fit the STM32F103C8T6 64 KB Flash budget.
"""

import argparse
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
from gif_to_c import generate, FRAME_BYTES

PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..'))
DEFAULT_OUTPUT_DIR = os.path.join(PROJECT_ROOT, 'Core', 'Inc')
DEFAULT_DEBUG_DIR = os.path.join(SCRIPT_DIR, 'debug_frames')


def main():
    parser = argparse.ArgumentParser(
        description='GIF → CH1116 OLED Animation C Header',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
    python tools/auto_gif.py meow.gif
    python tools/auto_gif.py meow.gif -n mycat -t 120 -m 24
    python tools/auto_gif.py cat.gif -o Core/Inc/custom.h --no-debug
        ''',
    )

    parser.add_argument(
        'gif_path',
        help='Path to the source GIF file',
    )

    parser.add_argument(
        '-n', '--name',
        default=None,
        help='Animation display name (default: derived from GIF filename)',
    )

    parser.add_argument(
        '-o', '--output',
        default=None,
        help='Output C header path (default: Core/Inc/<name>_anim.h)',
    )

    parser.add_argument(
        '-t', '--threshold',
        type=int, default=140,
        help='Binarization threshold 0-255, lower = fewer lit pixels (default: 140)',
    )

    parser.add_argument(
        '-m', '--max-frames',
        type=int, default=32,
        help='Target frame count limited by Flash budget (default: 32)',
    )

    parser.add_argument(
        '--bg',
        choices=['white', 'black'], default='white',
        help='Compositing canvas background color (default: white)',
    )

    parser.add_argument(
        '--debug-dir',
        default=DEFAULT_DEBUG_DIR,
        help='Directory for debug PNGs (default: tools/debug_frames/)',
    )

    parser.add_argument(
        '--no-debug',
        action='store_true',
        help='Disable debug frame output',
    )

    args = parser.parse_args()

    # Resolve GIF path
    gif_path = args.gif_path
    if not os.path.isabs(gif_path):
        gif_path = os.path.join(PROJECT_ROOT, gif_path)
    if not os.path.exists(gif_path):
        print(f"Error: GIF not found: {gif_path}")
        sys.exit(1)

    # Derive animation name from filename if not specified
    if args.name:
        anim_name = args.name
    else:
        anim_name = os.path.splitext(os.path.basename(gif_path))[0]

    # Derive C identifier (lowercase, safe chars, no leading/trailing underscores)
    c_name = re.sub(r'[^a-zA-Z0-9_]', '_', anim_name.lower())
    c_name = re.sub(r'_+', '_', c_name).strip('_')
    if not c_name or c_name[0].isdigit():
        c_name = '_' + c_name

    # Derive output path if not specified
    if args.output:
        output_h = args.output
        if not os.path.isabs(output_h):
            output_h = os.path.join(PROJECT_ROOT, output_h)
    else:
        output_h = os.path.join(DEFAULT_OUTPUT_DIR, f'{c_name}_anim.h')

    debug_dir = None if args.no_debug else args.debug_dir

    bg_color = (0, 0, 0) if args.bg == 'black' else (255, 255, 255)

    # ── Generate ──────────────────────────────────────────────────────────
    print("=" * 60)
    print("  MeowShow — GIF to OLED Animation Converter")
    print("=" * 60)
    print()

    try:
        from PIL import Image
    except ImportError:
        print("Error: Pillow is required. Install with: pip install pillow")
        sys.exit(1)

    header = generate(
        gif_path=gif_path,
        anim_name=c_name,
        threshold=args.threshold,
        bg_color=bg_color,
        max_frames=args.max_frames,
        debug_dir=debug_dir,
    )

    # Patch display name
    header = header.replace(f'.name        = "{c_name}"',
                            f'.name        = "{anim_name}"')

    # Write output
    os.makedirs(os.path.dirname(output_h), exist_ok=True)
    with open(output_h, 'w', encoding='utf-8') as f:
        f.write(header)

    # Stats
    frame_count = len(re.findall(r'static const uint8_t \w+_frame_\d+', header))
    total_kb = frame_count * FRAME_BYTES / 1024.0
    code_est_kb = 14.0

    print()
    print(f"  Output:  {output_h}")
    print(f"  Frames:  {frame_count}")
    print(f"  Data:    {total_kb:.1f} KB")
    print(f"  Code:    ~{code_est_kb:.0f} KB (estimated)")
    print(f"  Total:   ~{total_kb + code_est_kb:.0f} KB / 64 KB "
          f"({(total_kb + code_est_kb) / 64 * 100:.0f}%)")
    if debug_dir:
        print(f"\n  Debug frames saved to: {debug_dir}/")
        print(f"  (oled_*.png = binarized preview of what the OLED will show)")
    print()
    print("  Done! Rebuild and re-flash the project.")


if __name__ == '__main__':
    main()
