#!/usr/bin/env python3
"""Generate the 25x25 app launcher icon (front view of a metro car), white on
transparent. Stdlib only (zlib + struct)."""
import zlib, struct, os

W = H = 25
px = [[0] * W for _ in range(H)]


def setp(x, y):
    if 0 <= x < W and 0 <= y < H:
        px[y][x] = 255


def rect(x0, y0, x1, y1, v=255):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if 0 <= x < W and 0 <= y < H:
                px[y][x] = v


# Car body, front view — wide and balanced, with rounded corners.
rect(4, 4, 20, 20)
# Round the four corners.
for cx, cy in ((4, 4), (20, 4), (4, 20), (20, 20)):
    px[cy][cx] = 0
px[4][5] = px[4][19] = 255  # keep roof flat-ish but corners soft

# Windshield: one wide window split into two by a center pillar.
rect(6, 7, 18, 12, 0)
rect(11, 7, 13, 12, 255)

# Headlights (transparent notches near the bottom).
rect(6, 16, 8, 18, 0)
rect(16, 16, 18, 18, 0)

# Bumper line near the bottom of the car.
rect(6, 19, 18, 19, 255)

# Coupler under the car + a bit of rail.
rect(11, 21, 13, 22)
rect(3, 24, 21, 24)

def is_set(x, y):
    return 0 <= x < W and 0 <= y < H and px[y][x] == 255


# White fill + 1px black outline so the icon is visible on a white launcher.
raw = bytearray()
for y in range(H):
    raw.append(0)
    for x in range(W):
        if px[y][x] == 255:
            raw += bytes((255, 255, 255, 255))          # white body
        else:
            adj = any(is_set(x + dx, y + dy)
                      for dx in (-1, 0, 1) for dy in (-1, 0, 1))
            if adj:
                raw += bytes((0, 0, 0, 255))             # black stroke
            else:
                raw += bytes((0, 0, 0, 0))               # transparent


def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data +
            struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))


ihdr = struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)
png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
       chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))

out = os.path.join(os.path.dirname(__file__), "..", "resources", "images", "app_icon.png")
with open(out, "wb") as f:
    f.write(png)
print("wrote", os.path.normpath(out))
