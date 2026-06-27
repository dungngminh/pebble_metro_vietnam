#!/usr/bin/env python3
"""Generate Rebble appstore marketing assets: banner (720x320) and icons."""
import os
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.join(os.path.dirname(__file__), "..", "store")
os.makedirs(OUT, exist_ok=True)

ARIAL_BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
ARIAL = "/System/Library/Fonts/Supplemental/Arial.ttf"

BLUE = (14, 111, 184)
DARK = (8, 30, 52)

LINE_COLORS = [(230, 0, 0), (0, 176, 64), (0, 130, 224)]  # 2A, Nhon, HCMC


def rrect(draw, box, r, fill):
    draw.rounded_rectangle(box, radius=r, fill=fill)


def draw_metro(draw, x, y, w, h, body=(255, 255, 255), win=BLUE):
    """Front view of a metro car."""
    r = int(w * 0.18)
    rrect(draw, [x, y, x + w, y + h], r, body)
    # windshield split into two windows
    wm = int(w * 0.16)
    wt = int(y + h * 0.16)
    wb = int(y + h * 0.52)
    mid = x + w // 2
    gap = int(w * 0.06)
    draw.rounded_rectangle([x + wm, wt, mid - gap, wb], radius=int(w * 0.05), fill=win)
    draw.rounded_rectangle([mid + gap, wt, x + w - wm, wb], radius=int(w * 0.05), fill=win)
    # headlights
    hl = int(w * 0.12)
    hy = int(y + h * 0.66)
    draw.rounded_rectangle([x + wm, hy, x + wm + hl, hy + hl], radius=4, fill=win)
    draw.rounded_rectangle([x + w - wm - hl, hy, x + w - wm, hy + hl], radius=4, fill=win)


def vgradient(size, top, bot):
    img = Image.new("RGB", size, top)
    px = img.load()
    for j in range(size[1]):
        t = j / (size[1] - 1)
        c = tuple(int(top[k] + (bot[k] - top[k]) * t) for k in range(3))
        for i in range(size[0]):
            px[i, j] = c
    return img


# ---- banner 720x320 ----------------------------------------------------------
W, H = 720, 320
banner = vgradient((W, H), DARK, BLUE).convert("RGBA")
d = ImageDraw.Draw(banner)

title = ImageFont.truetype(ARIAL_BOLD, 56)
tag = ImageFont.truetype(ARIAL, 23)
small = ImageFont.truetype(ARIAL_BOLD, 19)

d.text((48, 78), "Metro VietNam", font=title, fill=(255, 255, 255))
d.text((50, 150), "Hanoi & HCMC metro, on your wrist", font=tag, fill=(200, 225, 245))

# Track line with the 3 line-colour station dots.
ty = 226
d.line([(60, ty), (410, ty)], fill=(120, 160, 195), width=4)
labels = ["2A", "Nhon", "HCMC"]
for i, c in enumerate(LINE_COLORS):
    cx = 74 + i * 140
    d.ellipse([cx - 11, ty - 11, cx + 11, ty + 11], fill=c)
    d.text((cx - 14, ty + 16), labels[i], font=small, fill=(220, 235, 248))

# Big train on the right.
draw_metro(d, 530, 84, 150, 158, body=(255, 255, 255), win=(20, 70, 120))
banner.convert("RGB").save(os.path.join(OUT, "banner-720x320.png"))

# ---- store icon 144x144 ------------------------------------------------------
for sz in (144, 48):
    icon = Image.new("RGBA", (sz, sz), (0, 0, 0, 0))
    di = ImageDraw.Draw(icon)
    rrect(di, [0, 0, sz - 1, sz - 1], int(sz * 0.22), BLUE)
    m = int(sz * 0.30)
    draw_metro(di, m, m, sz - 2 * m, sz - 2 * m - int(sz * 0.04),
               body=(255, 255, 255), win=BLUE)
    icon.save(os.path.join(OUT, f"icon-{sz}.png"))

print("wrote store/ banner + icons")
