#!/usr/bin/env python3
"""Generate a 25x25 white-on-transparent train pictogram PNG for the Pebble
timeline pin icon (tiny size). White silhouette so the OS can tint it.
Uses only the standard library (zlib + struct), no PIL."""
import zlib, struct, os

W = H = 25
px = [[0] * W for _ in range(H)]  # alpha 0/255


def setp(x, y):
    if 0 <= x < W and 0 <= y < H:
        px[y][x] = 255


def rect(x0, y0, x1, y1):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            setp(x, y)


def clear_rect(x0, y0, x1, y1):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if 0 <= x < W and 0 <= y < H:
                px[y][x] = 0


def disc(cx, cy, r):
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                setp(x, y)


# Train body (rounded rectangle), side view.
rect(4, 4, 20, 17)
# Round the top corners.
px[4][4] = px[4][20] = 0
px[4][5] = px[4][19] = 0
# Roof highlight curve (front nose rounded on the right).
px[5][20] = 0
px[16][4] = px[17][4] = 0  # round bottom-left a touch

# Windows (cut out -> transparent).
clear_rect(6, 7, 9, 11)
clear_rect(11, 7, 14, 11)
clear_rect(16, 7, 18, 11)
# Door line.
clear_rect(13, 12, 13, 16)
# Headlight (front, bottom-right) as a small notch.
clear_rect(19, 14, 20, 16)

# Wheels.
disc(8, 19, 2)
disc(16, 19, 2)
# Rail.
rect(3, 22, 21, 22)

# Encode PNG (RGBA, white where alpha set).
raw = bytearray()
for y in range(H):
    raw.append(0)  # filter type 0
    for x in range(W):
        a = px[y][x]
        raw += bytes((255, 255, 255, a))


def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data +
            struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))


ihdr = struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)
png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
       chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))

out = os.path.join(os.path.dirname(__file__), "..", "resources", "images", "train.png")
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "wb") as f:
    f.write(png)
print("wrote", os.path.normpath(out))
