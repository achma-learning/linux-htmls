#!/usr/bin/env python3
"""make-icons.py — draw the two .ico files the .exe files carry.

These are the icons Explorer, the taskbar and Alt+Tab show for the files
themselves. The icon in the notification area is drawn live by each program
(the prayer one moves with the day), so it is not here.

Every size is drawn from scratch at 8x and downsampled, rather than one big
image shrunk, so 16 px stays crisp. Needs Pillow:  python3 make-icons.py
"""
import os
from PIL import Image, ImageDraw

SIZES = [16, 20, 24, 32, 40, 48, 64, 256]
SS = 8
HERE = os.path.dirname(os.path.abspath(__file__))

INK3, LINE2, DIM = (0x1D, 0x24, 0x30), (0x33, 0x3D, 0x4D), (0x81, 0x8C, 0x9E)
SAFF, VERD, TERRA = (0xE8, 0xA3, 0x3D), (0x4F, 0xB0, 0x9A), (0xC9, 0x6A, 0x4E)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def prayer(n):
    z = n * SS
    u = z / 16
    im = Image.new("RGBA", (z, z), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0.5 * u, 0.5 * u, z - 0.5 * u, z - 0.5 * u], radius=3.2 * u,
                        fill=INK3 + (255,), outline=LINE2 + (255,), width=max(1, int(u)))
    top, bot, rx, rw = 2.6 * u, z - 2.6 * u, 5.2 * u, max(SS, 1.6 * u)
    d.rectangle([rx, top, rx + rw, bot], fill=LINE2 + (255,))
    now = top + 0.62 * (bot - top)
    steps = 40
    for i in range(steps):           # verdigris → saffron, what is lived of the day
        y0 = top + (now - top) * i / steps
        y1 = top + (now - top) * (i + 1) / steps
        d.rectangle([rx, y0, rx + rw, y1], fill=lerp(VERD, SAFF, i / steps) + (255,))
    for f, nxt in ((0.06, 0), (0.18, 0), (0.45, 0), (0.6, 0), (0.78, 1), (0.9, 0)):
        y = top + f * (bot - top)
        ln, th = (7 * u, 2 * u) if nxt else (4.5 * u, u)
        d.rectangle([rx + rw + u, y - th / 2, rx + rw + u + ln, y + th / 2],
                    fill=(TERRA if nxt else DIM) + (255,))
    r = 2.2 * u
    cx = rx + rw / 2
    d.ellipse([cx - r, now - r, cx + r, now + r], fill=SAFF + (255,))
    return im.resize((n, n), Image.LANCZOS)


def aihub(n):
    z = n * SS
    grad = Image.new("RGBA", (z, z))
    a, b = (0x7c, 0x5c, 0xff), (0x4f, 0x8c, 0xff)
    px = grad.load()
    for y in range(z):
        for x in range(z):
            px[x, y] = lerp(a, b, (x + y) / (2 * z)) + (255,)
    mask = Image.new("L", (z, z), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0.5 * SS, 0.5 * SS, z - 0.5 * SS, z - 0.5 * SS],
                                           radius=z * 0.28, fill=255)
    im = Image.new("RGBA", (z, z), (0, 0, 0, 0))
    im.paste(grad, (0, 0), mask)
    c, R, r = z / 2, z * 0.34, z * 0.085
    star = [(c, c - R), (c + r, c - r), (c + R, c), (c + r, c + r),
            (c, c + R), (c - r, c + r), (c - R, c), (c - r, c - r)]
    ImageDraw.Draw(im).polygon(star, fill=(255, 255, 255, 255))
    return im.resize((n, n), Image.LANCZOS)


def save(draw, name):
    imgs = [draw(n) for n in SIZES]
    path = os.path.join(HERE, "icons", name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    imgs[-1].save(path, format="ICO", sizes=[(n, n) for n in SIZES], append_images=imgs[:-1])
    print("wrote", path)


if __name__ == "__main__":
    save(prayer, "prayer-tray.ico")
    save(aihub, "ai-hub-tray.ico")
