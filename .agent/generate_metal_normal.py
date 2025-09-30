#!/usr/bin/env python3
"""Generate a tileable brushed-metal normal map as PNG."""
from __future__ import annotations

import math
import struct
import zlib

WIDTH = 512
HEIGHT = 512
AMPLITUDE_X = 0.25  # tangent perturbation along x (brushing direction)
AMPLITUDE_Y = 0.18  # tangent perturbation along y
FREQUENCY_X = 24.0
FREQUENCY_Y = 14.0
NOISE_FREQUENCY = 7.0
PHASE_SHIFT = 1.7


def normal_components(u: float, v: float) -> tuple[float, float, float]:
    """Compute tangent-space normal components from procedural functions."""
    stripe = math.sin(u * math.tau * FREQUENCY_X)
    swirl = math.sin(v * math.tau * FREQUENCY_Y + stripe * 0.35 + PHASE_SHIFT)
    micro = math.sin((u + v) * math.tau * NOISE_FREQUENCY + stripe * 0.5)

    nx = (stripe * (1.0 - abs(swirl) * 0.35) + micro * 0.15) * AMPLITUDE_X
    ny = (swirl + micro * 0.25) * AMPLITUDE_Y
    nz_sq = max(0.0, 1.0 - nx * nx - ny * ny)
    nz = math.sqrt(nz_sq)
    return nx, ny, nz


def encode_row(y: int) -> bytes:
    """Encode a single scanline with PNG filter type 0."""
    row = bytearray([0])  # no filter
    v = y / (HEIGHT - 1)
    for x in range(WIDTH):
        u = x / (WIDTH - 1)
        nx, ny, nz = normal_components(u, v)
        r = int((nx * 0.5 + 0.5) * 255.0 + 0.5)
        g = int((ny * 0.5 + 0.5) * 255.0 + 0.5)
        b = int((nz * 0.5 + 0.5) * 255.0 + 0.5)
        a = 255
        row.extend((r & 0xFF, g & 0xFF, b & 0xFF, a))
    return bytes(row)


def build_png(pixel_rows: list[bytes]) -> bytes:
    signature = b"\x89PNG\r\n\x1a\n"

    def chunk(tag: bytes, data: bytes) -> bytes:
        length = struct.pack('>I', len(data))
        crc = struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
        return length + tag + data + crc

    ihdr = struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 6, 0, 0, 0)
    ihdr_chunk = chunk(b'IHDR', ihdr)

    raw = b''.join(pixel_rows)
    compressor = zlib.compressobj(level=9)
    compressed = compressor.compress(raw) + compressor.flush()
    idat_chunk = chunk(b'IDAT', compressed)

    iend_chunk = chunk(b'IEND', b'')

    return signature + ihdr_chunk + idat_chunk + iend_chunk


def main() -> None:
    rows = [encode_row(y) for y in range(HEIGHT)]
    png_bytes = build_png(rows)
    with open('assets/textures/metal_normal.png', 'wb') as handle:
        handle.write(png_bytes)


if __name__ == '__main__':
    main()
