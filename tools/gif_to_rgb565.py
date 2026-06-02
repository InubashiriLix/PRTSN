#!/usr/bin/env python3

import argparse
import bisect
import re
from pathlib import Path

try:
    from PIL import Image, ImageSequence
except ImportError as exc:
    raise SystemExit("Pillow is required. Install with: python3 -m pip install Pillow") from exc


def parse_color(value: str) -> tuple[int, int, int]:
    match = re.fullmatch(r"#?([0-9a-fA-F]{6})", value.strip())
    if not match:
        raise argparse.ArgumentTypeError("color must be RRGGBB or #RRGGBB")

    raw = match.group(1)
    return int(raw[0:2], 16), int(raw[2:4], 16), int(raw[4:6], 16)


def rgb_to_rgb565_be_bytes(rgb: bytes) -> bytearray:
    out = bytearray(len(rgb) // 3 * 2)
    dst = 0
    for src in range(0, len(rgb), 3):
        r, g, b = rgb[src], rgb[src + 1], rgb[src + 2]
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out[dst] = (value >> 8) & 0xFF
        out[dst + 1] = value & 0xFF
        dst += 2
    return out


def fit_frame(frame: Image.Image, width: int, height: int, fit: str, background: tuple[int, int, int]) -> Image.Image:
    frame = frame.convert("RGBA")

    if fit == "stretch":
        resized = frame.resize((width, height), Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (width, height), (*background, 255))
        canvas.alpha_composite(resized)
        return canvas.convert("RGB")

    src_w, src_h = frame.size
    scale = max(width / src_w, height / src_h) if fit == "cover" else min(width / src_w, height / src_h)
    new_w = max(1, round(src_w * scale))
    new_h = max(1, round(src_h * scale))
    resized = frame.resize((new_w, new_h), Image.Resampling.LANCZOS)

    canvas = Image.new("RGBA", (width, height), (*background, 255))
    x = (width - new_w) // 2
    y = (height - new_h) // 2
    canvas.alpha_composite(resized, (x, y))
    return canvas.convert("RGB")


def load_original_frames(path: Path, width: int, height: int, fit: str, background: tuple[int, int, int]):
    with Image.open(path) as gif:
        frames = []
        durations = []

        for frame in ImageSequence.Iterator(gif):
            duration_ms = int(frame.info.get("duration", 100))
            durations.append(max(10, duration_ms))
            fitted = fit_frame(frame, width, height, fit, background)
            frames.append(rgb_to_rgb565_be_bytes(fitted.tobytes()))

    if not frames:
        raise SystemExit(f"{path} has no frames")

    return frames, durations


def resample_frames(frames: list[bytearray], durations: list[int], fps: int):
    frame_interval_ms = max(1, round(1000 / fps))
    total_ms = sum(durations)
    source_timeline = []
    elapsed = 0
    for duration in durations:
        elapsed += duration
        source_timeline.append(elapsed)

    out_frames = []
    out_durations = []
    t = 0
    while t < total_ms:
        index = min(bisect.bisect_right(source_timeline, t), len(frames) - 1)
        out_frames.append(frames[index])
        out_durations.append(frame_interval_ms)
        t += frame_interval_ms

    return out_frames, out_durations


def write_header(path: Path, name: str, width: int, height: int, fps: int, frames: list[bytearray], durations: list[int]):
    path.parent.mkdir(parents=True, exist_ok=True)
    frame_bytes = width * height * 2
    with path.open("w", encoding="utf-8") as out:
        out.write("#pragma once\n\n")
        out.write("#include <Arduino.h>\n")
        out.write("#include <cstddef>\n")
        out.write("#include <cstdint>\n\n")
        out.write(f"namespace {name}\n")
        out.write("{\n")
        out.write(f"constexpr uint16_t Width = {width};\n")
        out.write(f"constexpr uint16_t Height = {height};\n")
        out.write(f"constexpr uint16_t FrameCount = {len(frames)};\n")
        out.write(f"constexpr uint16_t Fps = {fps};\n")
        out.write(f"constexpr size_t FrameBytes = {frame_bytes};\n\n")

        out.write("constexpr uint16_t DurationsMs[] = {")
        out.write(", ".join(str(duration) for duration in durations))
        out.write("};\n\n")

        out.write("alignas(4) const uint8_t Frames[] PROGMEM = {\n")
        for frame_index, frame in enumerate(frames):
            if len(frame) != frame_bytes:
                raise SystemExit(f"frame {frame_index} has {len(frame)} bytes, expected {frame_bytes}")
            out.write(f"    // frame {frame_index}\n")
            for offset in range(0, len(frame), 16):
                row = ", ".join(f"0x{value:02X}" for value in frame[offset:offset + 16])
                out.write(f"    {row},\n")
        out.write("};\n")
        out.write("}\n")


def write_binary(path: Path, frames: list[bytearray]):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as out:
        for frame in frames:
            out.write(frame)


def average_fps(durations: list[int]) -> int:
    total = sum(durations)
    if total <= 0:
        return 0
    return max(1, round(len(durations) * 1000 / total))


def main():
    parser = argparse.ArgumentParser(description="Convert GIF frames to big-endian RGB565 LCD data.")
    parser.add_argument("gif", type=Path)
    parser.add_argument("-o", "--output", type=Path, required=True)
    parser.add_argument("--name", default="LcdAnimation")
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--fps", type=int, default=60)
    parser.add_argument("--fit", choices=("contain", "cover", "stretch"), default="contain")
    parser.add_argument("--background", type=parse_color, default=(0, 0, 0))
    parser.add_argument("--resample", action="store_true", help="resample to --fps instead of preserving GIF frame durations")
    parser.add_argument("--format", choices=("header", "bin"), default="header")
    args = parser.parse_args()

    if args.width <= 0 or args.height <= 0:
        raise SystemExit("--width and --height must be positive")
    if args.fps <= 0:
        raise SystemExit("--fps must be positive")

    frames, durations = load_original_frames(args.gif, args.width, args.height, args.fit, args.background)
    metadata_fps = average_fps(durations)

    if args.resample:
        frames, durations = resample_frames(frames, durations, args.fps)
        metadata_fps = args.fps

    if args.format == "header":
        write_header(args.output, args.name, args.width, args.height, metadata_fps, frames, durations)
    else:
        write_binary(args.output, frames)

    print(
        f"converted {args.gif} -> {args.output}: "
        f"{args.width}x{args.height}, frames={len(frames)}, fps={metadata_fps}, bytes/frame={args.width * args.height * 2}"
    )


if __name__ == "__main__":
    main()
