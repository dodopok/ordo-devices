"""Paints a stand-in picture for the simulator: the celebration page needs a
466 x 466 JPEG like the ones the content host sends, and the real ones belong
to the CMS, not to this repository.
   make_art.py <out.jpg>
It is saved the way the host saves them: baseline, 4:2:0."""
import math
import sys

from PIL import Image, ImageDraw, ImageFilter

SIDE = 466
image = Image.new("RGB", (SIDE, SIDE))
pixels = image.load()
for y in range(SIDE):
    for x in range(SIDE):
        # A dusk sky behind an arch: deep blue at the top, ochre near the ground.
        t = y / SIDE
        glow = math.exp(-(((x - SIDE / 2) / 190.0) ** 2 + ((y - 150) / 170.0) ** 2))
        r = int(26 + 120 * t * t + 70 * glow)
        g = int(34 + 62 * t + 52 * glow)
        b = int(66 + 20 * (1 - t) + 10 * glow)
        pixels[x, y] = (min(r, 255), min(g, 255), min(b, 255))
draw = ImageDraw.Draw(image)
# The arch.
draw.rectangle((40, 150, 86, SIDE), fill=(92, 70, 48))
draw.rectangle((380, 150, 426, SIDE), fill=(92, 70, 48))
draw.arc((40, -40, 426, 346), 180, 360, fill=(120, 94, 62), width=46)
# A halo, a head, a cloak cut in two.
draw.ellipse((158, 62, 308, 212), fill=(232, 201, 104))
draw.ellipse((168, 72, 298, 202), fill=(214, 178, 82))
draw.ellipse((196, 104, 270, 186), fill=(206, 160, 122))
draw.polygon([(140, 466), (176, 214), (290, 214), (326, 466)], fill=(150, 44, 40))
draw.polygon([(233, 214), (290, 214), (326, 466), (233, 466)], fill=(120, 34, 34))
draw.line((233, 214, 233, 466), fill=(232, 201, 104), width=3)
image = image.filter(ImageFilter.GaussianBlur(0.6))
image.save(sys.argv[1], "JPEG", quality=76, optimize=True, progressive=False, subsampling="4:2:0")
