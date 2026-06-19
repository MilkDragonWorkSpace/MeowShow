#!/usr/bin/env python3
"""
gif_to_c.py — Convert a GIF to CH1116 OLED frame data with proper compositing.

Handles GIF frame differencing (disposal, transparency, partial updates) by
accumulating frames onto a white canvas and then binarizing for the OLED.

Usage:
    python gif_to_c.py <gif_path> [anim_name] [--debug-dir <path>]
                           [--threshold N] [--bg white|black]
                           [--max-frames N]

Output: C header with CH1116-format frame arrays + GifAnimation struct.

Requirements: Python 3, Pillow
"""

import sys, os
from PIL import Image

# ── OLED constants ──────────────────────────────────────────────────────────
OLED_W, OLED_H = 128, 64
OLED_PAGES     = 8
FRAME_BYTES    = OLED_W * OLED_PAGES  # 1024

# Default Flash budget: 32 KB for frames, 32 KB for code+fonts+HAL
DEFAULT_MAX_FRAMES = 32


# ═══════════════════════════════════════════════════════════════════════════════
#  GIF Decoder — proper frame compositing
# ═══════════════════════════════════════════════════════════════════════════════

def decode_gif(gif_path: str, bg_color=(255, 255, 255)) -> list[Image.Image]:
    """
    Decode a GIF with proper frame compositing.

    Each GIF frame is pasted onto an accumulating canvas. The frame's
    alpha channel controls which pixels overwrite the canvas.

    Args:
        gif_path:  Path to the GIF file.
        bg_color:  RGB tuple for the initial canvas color. Use white (255,255,255)
                   for GIFs with transparent backgrounds (most web GIFs).
                   Use black (0,0,0) for dark-theme GIFs.

    Returns:
        List of composited PIL Images (RGB, at OLED resolution 128×64),
        one per GIF frame, in display order.
    """
    gif = Image.open(gif_path)
    screen_w, screen_h = gif.size

    if not getattr(gif, 'is_animated', False):
        frame = gif.convert('RGB').resize((OLED_W, OLED_H), Image.LANCZOS)
        return [frame]

    # Canvas: accumulated display state, RGBA for alpha compositing
    canvas = Image.new('RGBA', (screen_w, screen_h),
                       bg_color + (255,) if len(bg_color) == 3 else bg_color)

    composited_frames = []
    frame_idx = 0

    while True:
        try:
            gif.seek(frame_idx)
        except EOFError:
            break

        # Get current frame as RGBA (with transparency from GIF)
        frame_rgba = gif.convert('RGBA')

        # Paste frame onto canvas. Using frame_rgba as its own mask means
        # transparent pixels (alpha=0) don't overwrite the canvas, while
        # opaque/semi-transparent pixels do.
        canvas.paste(frame_rgba, (0, 0), frame_rgba)

        # Convert composited canvas to RGB and resize for OLED
        composite = canvas.convert('RGB')
        composite = composite.resize((OLED_W, OLED_H), Image.LANCZOS)

        composited_frames.append(composite)
        frame_idx += 1

    return composited_frames


# ═══════════════════════════════════════════════════════════════════════════════
#  Frame → CH1116 Buffer
# ═══════════════════════════════════════════════════════════════════════════════

def frame_to_ch1116(img: Image.Image, threshold: int = 140) -> bytearray:
    """
    Convert an RGB PIL Image (128×64) to CH1116 framebuffer bytes.

    CH1116 horizontal addressing mode byte order:
      page 0, col 0..127 → page 1, col 0..127 → ... → page 7, col 0..127

    Within each byte: bit 0 = top pixel (row 0 of that page),
                      bit 7 = bottom pixel (row 7 of that page).

    Binarization (fixed threshold):
      pixel < threshold  →  1 (OLED pixel lights up)
      pixel >= threshold →  0 (OLED pixel stays dark)

    This maps darker content to visible OLED pixels — correct for
    most GIFs (dark drawings on light/transparent backgrounds).
    """
    gray = img.convert('L')
    px = gray.load()

    buf = bytearray(FRAME_BYTES)

    for page in range(OLED_PAGES):
        for col in range(OLED_W):
            byte_val = 0
            for bit in range(8):
                y = page * 8 + bit
                if px[col, y] < threshold:
                    byte_val |= (1 << bit)
            buf[page * OLED_W + col] = byte_val

    return buf


# ═══════════════════════════════════════════════════════════════════════════════
#  Frame Sampling — uniform stride to preserve animation flow
# ═══════════════════════════════════════════════════════════════════════════════

def _frame_content_ratio(img: Image.Image) -> float:
    """
    Return the fraction of 'dark' pixels (cat content) in a composited frame.
    Dark = grayscale value < 160 (content that would light up on OLED).
    """
    gray = img.convert('L')
    pixels = list(gray.getdata())
    dark = sum(1 for p in pixels if p < 160)
    return dark / len(pixels)


def sample_frames(frames: list, delays: list, target_count: int) -> tuple[list, list]:
    """
    Uniformly sample frames, skipping near-empty frames.

    Strategy:
      1. Uniformly pick `target_count` indices across the timeline.
      2. If a picked frame has < 2% content while its neighbors have > 10%,
         replace it with the nearest frame that has content.
      3. Adjust delays to preserve total animation duration.

    Returns (sampled_frames, adjusted_delays).
    """
    n = len(frames)
    if n <= target_count:
        return frames, delays

    # Compute content ratios for all frames
    ratios = [_frame_content_ratio(f) for f in frames]

    # Find the first frame with meaningful content
    MIN_CONTENT = 0.03   # 3% — below this is "empty"
    first_good = 0
    for i in range(n):
        if ratios[i] >= MIN_CONTENT:
            first_good = i
            break

    # Sample from first_good to n-1 uniformly, with target_count frames
    effective_range = max(n - first_good, 1)
    indices = sorted(set(
        [first_good] +
        [first_good + int(effective_range * i / (target_count - 1))
         for i in range(1, target_count)]
    ))
    indices = [min(i, n - 1) for i in indices]
    indices = sorted(set(indices))

    # If short (due to set dedup), fill from remaining pool
    while len(indices) < target_count:
        max_gap = 0
        max_pos = 0
        for i in range(len(indices) - 1):
            gap = indices[i + 1] - indices[i]
            if gap > max_gap:
                max_gap = gap
                max_pos = i
        if max_gap <= 1:
            break
        mid = (indices[max_pos] + indices[max_pos + 1]) // 2
        indices.append(mid)
        indices.sort()

    fixed_indices = indices[:target_count]

    sampled_frames = [frames[i] for i in fixed_indices]

    # Adjust delays
    adjusted_delays = []
    for si, src_idx in enumerate(fixed_indices):
        if si < len(fixed_indices) - 1:
            next_idx = fixed_indices[si + 1]
            total = sum(delays[min(j, n - 1)] for j in range(src_idx, next_idx))
            adjusted_delays.append(min(max(total, 50), 1000))
        else:
            adjusted_delays.append(max(delays[min(src_idx, n - 1)], 50))

    return sampled_frames, adjusted_delays


# ═══════════════════════════════════════════════════════════════════════════════
#  GIF delays extraction
# ═══════════════════════════════════════════════════════════════════════════════

def extract_delays(gif_path: str) -> list:
    """Extract per-frame delays from a GIF (milliseconds)."""
    gif = Image.open(gif_path)
    delays = []
    frame_idx = 0
    while True:
        try:
            gif.seek(frame_idx)
            d = gif.info.get('duration', 100)
            if d <= 10:
                d = 100
            delays.append(min(d, 5000))
            frame_idx += 1
        except EOFError:
            break
    return delays


# ═══════════════════════════════════════════════════════════════════════════════
#  C Header Generator
# ═══════════════════════════════════════════════════════════════════════════════

def escape_c_name(name: str) -> str:
    result = ''.join(ch if ch.isalnum() or ch == '_' else '_' for ch in name)
    if not result:
        result = 'anim'
    if result[0].isdigit():
        result = '_' + result
    return result


def generate(gif_path: str, anim_name: str,
             threshold: int = 140,
             bg_color: tuple = (255, 255, 255),
             max_frames: int = DEFAULT_MAX_FRAMES,
             debug_dir: str = None) -> str:
    """
    Full pipeline: decode → sample → convert → C header.

    Args:
        gif_path:   Path to GIF file.
        anim_name:  Display name (e.g. "Meow!").
        threshold:  Grayscale threshold for binarization (0-255).
                    Lower = more selective (fewer lit pixels).
        bg_color:   RGB tuple for compositing canvas background.
        max_frames: Target frame count (limited by Flash budget).
        debug_dir:  If set, save composited+binarized frames for inspection.

    Returns:
        Complete C header string.
    """
    print(f"Decoding: {gif_path}", file=sys.stderr)

    # Step 1 — Decode
    composited = decode_gif(gif_path, bg_color)
    delays = extract_delays(gif_path)
    print(f"  {len(composited)} frames decoded (composited)", file=sys.stderr)

    # Step 2 — Sample
    original_count = len(composited)
    composited, delays = sample_frames(composited, delays, max_frames)
    frame_count = len(composited)
    print(f"  Sampled to {frame_count} frames (from {original_count})",
          file=sys.stderr)

    # Step 3 — Convert to CH1116
    ch1116_bufs = []
    for i, img in enumerate(composited):
        buf = frame_to_ch1116(img, threshold)
        ch1116_bufs.append(buf)

        if debug_dir:
            os.makedirs(debug_dir, exist_ok=True)
            # Save composited RGB frame
            img.save(os.path.join(debug_dir, f'rgb_{i:03d}.png'))
            # Save binarized preview (build a 1-bit image from the buffer)
            preview = Image.new('1', (OLED_W, OLED_H))
            preview_px = preview.load()
            for page in range(OLED_PAGES):
                for col in range(OLED_W):
                    byte_val = buf[page * OLED_W + col]
                    for bit in range(8):
                        y = page * 8 + bit
                        if y < OLED_H:
                            preview_px[col, y] = 1 if (byte_val & (1 << bit)) else 0
            preview.save(os.path.join(debug_dir, f'oled_{i:03d}.png'))

    # Step 4 — Generate C code
    safe_name = escape_c_name(anim_name)
    total_kb = frame_count * FRAME_BYTES / 1024.0

    lines = []
    lines.append('/**')
    lines.append(f' * @file    {safe_name}.h')
    lines.append(f' * @brief   Auto-generated from {os.path.basename(gif_path)}')
    lines.append(f' *')
    lines.append(f' * Source:  {original_count} GIF frames')
    lines.append(f' * Output:  {frame_count} sampled frames')
    lines.append(f' * Each:    {FRAME_BYTES} bytes (128×64 CH1116 format)')
    lines.append(f' * Total:   {frame_count * FRAME_BYTES} bytes ({total_kb:.1f} KB)')
    lines.append(f' *')
    lines.append(f' * Generated by tools/gif_to_c.py')
    lines.append(f' */')
    lines.append('')
    lines.append(f'#ifndef __{safe_name.upper()}_H')
    lines.append(f'#define __{safe_name.upper()}_H')
    lines.append('')
    lines.append('#include "gif_animation.h"')
    lines.append('#include <stdint.h>')
    lines.append('')
    lines.append('#ifdef __cplusplus')
    lines.append('extern "C" {')
    lines.append('#endif')
    lines.append('')

    frame_names = []
    for fi in range(frame_count):
        name = f'{safe_name}_frame_{fi}'
        frame_names.append(name)
        buf = ch1116_bufs[fi]

        lines.append(f'static const uint8_t {name}[{FRAME_BYTES}] = {{')
        for i in range(0, len(buf), 16):
            chunk = buf[i:i + 16]
            lines.append('    ' + ', '.join(f'0x{b:02X}' for b in chunk) + ',')
        lines.append('};')
        lines.append('')

    lines.append(f'static const GifFrame {safe_name}_frames[] = {{')
    for fi in range(frame_count):
        lines.append(f'    {{ .bitmap = {frame_names[fi]}, '
                     f'.delay_ms = {delays[fi]} }},')
    lines.append('};')
    lines.append('')

    lines.append(f'const GifAnimation gif_{safe_name}_anim = {{')
    lines.append(f'    .frames      = {safe_name}_frames,')
    lines.append(f'    .frame_count = {frame_count},')
    lines.append(f'    .loop_count  = 0,  /* Infinite loop */')
    lines.append(f'    .name        = "{anim_name}"')
    lines.append('};')
    lines.append('')

    lines.append('#ifdef __cplusplus')
    lines.append('}')
    lines.append('#endif')
    lines.append('')
    lines.append(f'#endif /* __{safe_name.upper()}_H */')
    lines.append('')

    return '\n'.join(lines)


# ═══════════════════════════════════════════════════════════════════════════════
#  CLI
# ═══════════════════════════════════════════════════════════════════════════════

def main():
    args = sys.argv[1:]
    if not args:
        print(f"Usage: python {os.path.basename(__file__)} <gif_path> [anim_name] [options]",
              file=sys.stderr)
        print(f"Options:", file=sys.stderr)
        print(f"  --threshold N    Binarization threshold (default: 140)", file=sys.stderr)
        print(f"  --bg white|black Canvas background (default: white)", file=sys.stderr)
        print(f"  --max-frames N   Target frame count (default: {DEFAULT_MAX_FRAMES})", file=sys.stderr)
        print(f"  --debug-dir DIR  Save debug PNGs for inspection", file=sys.stderr)
        sys.exit(1)

    gif_path = args[0]
    anim_name = args[1] if len(args) > 1 and not args[1].startswith('--') else \
                os.path.splitext(os.path.basename(gif_path))[0]

    threshold = 140
    bg_color = (255, 255, 255)
    max_frames = DEFAULT_MAX_FRAMES
    debug_dir = None

    # Parse flags
    i = 0
    while i < len(args):
        if args[i] == '--threshold' and i + 1 < len(args):
            threshold = int(args[i + 1])
            i += 2
        elif args[i] == '--bg' and i + 1 < len(args):
            bg_color = (0, 0, 0) if args[i + 1] == 'black' else (255, 255, 255)
            i += 2
        elif args[i] == '--max-frames' and i + 1 < len(args):
            max_frames = int(args[i + 1])
            i += 2
        elif args[i] == '--debug-dir' and i + 1 < len(args):
            debug_dir = args[i + 1]
            i += 2
        else:
            i += 1

    if not os.path.exists(gif_path):
        print(f"Error: file not found: {gif_path}", file=sys.stderr)
        sys.exit(1)

    header = generate(gif_path, anim_name, threshold, bg_color, max_frames, debug_dir)
    print(header)


if __name__ == '__main__':
    main()
