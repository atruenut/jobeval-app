#!/usr/bin/env python3
"""Draws the app icon (original artwork: a clock face with a dollar sign on blue) and writes
assets/icon.png and assets/jobeval.ico.   python3 tools/make_icon.py   (needs Pillow)"""
import os
from PIL import Image, ImageDraw, ImageFont

S = 1024
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
d.rounded_rectangle([40, 40, S - 40, S - 40], radius=200, fill=(30, 95, 180, 255))
c = S // 2
d.ellipse([c - 360, c - 360, c + 360, c + 360], fill=(255, 255, 255, 255))
d.ellipse([c - 320, c - 320, c + 320, c + 320], fill=(236, 243, 252, 255))
for i in range(12):   # hour ticks
    import math
    a = math.radians(i * 30)
    r1, r2 = 300, 330 if i % 3 else 345
    d.line([c + r1 * math.sin(a), c - r1 * math.cos(a), c + r2 * math.sin(a), c - r2 * math.cos(a)], fill=(30, 95, 180, 255), width=14)
font = None
for f in ("DejaVuSans-Bold.ttf", "arialbd.ttf", "Arial Bold.ttf"):
    try:
        font = ImageFont.truetype(f, 420); break
    except OSError:
        pass
if font:
    d.text((c, c + 10), "$", font=font, fill=(30, 95, 180, 255), anchor="mm")
out = os.path.join(os.path.dirname(__file__), "..", "assets")
os.makedirs(out, exist_ok=True)
img.resize((256, 256), Image.LANCZOS).save(os.path.join(out, "icon.png"))
img.save(os.path.join(out, "jobeval.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
