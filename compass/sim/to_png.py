"""Turns the simulator's raw frames into PNGs with the round glass cut out,
plus contact sheets that show the screens side by side."""
import glob
import os
import sys

from PIL import Image, ImageDraw

out = sys.argv[1]
SIZE = 466
names = []
for path in sorted(glob.glob(os.path.join(out, "*.ppm"))):
    frame = Image.open(path).convert("RGB")
    mask = Image.new("L", (SIZE * 4, SIZE * 4), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, SIZE * 4 - 1, SIZE * 4 - 1), fill=255)
    mask = mask.resize((SIZE, SIZE), Image.LANCZOS)
    round_ = Image.new("RGB", (SIZE, SIZE), (18, 18, 20))
    round_.paste(frame, (0, 0), mask)
    png = path[:-4] + ".png"
    round_.save(png)
    os.remove(path)
    names.append(png)

# Contact sheets, eight screens each.
PAD, COLS = 24, 4
for start in range(0, len(names), 8):
    group = names[start:start + 8]
    rows = (len(group) + COLS - 1) // COLS
    sheet = Image.new("RGB", (COLS * (SIZE + PAD) + PAD, rows * (SIZE + PAD + 26) + PAD), (18, 18, 20))
    draw = ImageDraw.Draw(sheet)
    for i, png in enumerate(group):
        x = PAD + (i % COLS) * (SIZE + PAD)
        y = PAD + (i // COLS) * (SIZE + PAD + 26)
        sheet.paste(Image.open(png), (x, y))
        draw.text((x + 8, y + SIZE + 6), os.path.basename(png)[:-4], fill=(150, 150, 150))
    sheet.save(os.path.join(out, "sheet-%d.png" % (start // 8 + 1)))
print("%d screens" % len(names))
