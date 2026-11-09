"""Strings the simulator's movie frames into an animated GIF.
   to_gif.py <frames-dir> <out.gif> [size] [every]
`every` keeps one frame in so many: the simulator steps at 30 a second."""
import glob
import os
import sys

from PIL import Image, ImageDraw

frames_dir, target = sys.argv[1], sys.argv[2]
size = int(sys.argv[3]) if len(sys.argv) > 3 else 320
every = int(sys.argv[4]) if len(sys.argv) > 4 else 2
SIZE = 466

mask = Image.new("L", (SIZE * 2, SIZE * 2), 0)
ImageDraw.Draw(mask).ellipse((0, 0, SIZE * 2 - 1, SIZE * 2 - 1), fill=255)
mask = mask.resize((SIZE, SIZE), Image.LANCZOS)

frames = []
paths = sorted(glob.glob(os.path.join(frames_dir, "f*.ppm")))
for path in paths[::every]:
    frame = Image.open(path).convert("RGB")
    round_ = Image.new("RGB", (SIZE, SIZE), (18, 18, 20))
    round_.paste(frame, (0, 0), mask)
    frames.append(round_.resize((size, size), Image.LANCZOS))
for path in paths:
    os.remove(path)
if not frames:
    sys.exit("no frames in %s" % frames_dir)
frames[0].save(target, save_all=True, append_images=frames[1:], duration=33 * every, loop=0, optimize=False)
print("%d frames, %.1f s, %s" % (len(frames), len(frames) * 33 * every / 1000.0, target))
