from PIL import Image, ImageDraw
import numpy as np

import math
import os
import spectra

import grf


SAFE_COLORS = list(range(1, 0xD7))
f = open("../ttd-newgrf-dos.gpl")

while f.readline().strip() != "#":
    pass


colors = []
for _ in range(256):
    try:
        r, g, b, _, i = f.readline().split()
    except ValueError:
        break
    c = spectra.rgb(float(r) / 255., float(g) / 255., float(b) / 255.)
    # if c in SAFE_COLORS:
    colors.append((int(i), c))

def color_distance(c1, c2):
    rmean = (c1.rgb[0] + c2.rgb[0]) / 2.
    r = c1.rgb[0] - c2.rgb[0]
    g = c1.rgb[1] - c2.rgb[1]
    b = c1.rgb[2] - c2.rgb[2]
    return math.sqrt(
        ((2 + rmean) * r * r) +
        4 * g * g +
        (3 - rmean) * b * b)

def find_best_color(x):
    mj, md = 0, 1e100
    for j, c in colors:
        if j not in SAFE_COLORS:
            continue
        d = color_distance(x, c)
        if d < md:
            mj, md = j, d
    return mj

def gen_recolor(color_func):
    out = np.arange(256, dtype=np.uint8)
    for i, c in colors:
        if i not in SAFE_COLORS:
            continue
        out[i] = find_best_color(color_func(c))
    return out

def gen_tint(tint, ratio):
    return lambda x: find_best_color(x.blend(tint, ratio=ratio))

def gen_brightness(level):
    def func(x):
        if level > 0:
            return x.brighten(amount=2.56 * level)
        else:
            return x.darken(amount=-2.56 * level)
    return gen_recolor(func)

# def gen_land_recolor():
#     def func(x):
#         if 2 * g > r + b:
#             x = x.blend(spectra.rgb(0.7, 1, 0), ratio=0.2)
#         else:
#             x = x.saturate(20)
#         return x
#     return gen_recolor(func)

def gen_land_recolor():
    def func(x):
        r, g, b = x.rgb
        if 2 * g > r + b:
            x = x.blend(spectra.rgb(0.7, 1, 0), ratio=0.05)
            x = x.saturate(10)
        # elif 3 * b > r + g:
        #     x = x.blend(spectra.rgb(0, 0, 1), ratio=0.3)
        #     x = x.blend(spectra.rgb(1, 0, 1), ratio=0.5)
        #     x = x.saturate(40)
        else:
            x = x.blend(spectra.rgb(0.7, 1, 0), ratio=0.05)
            x = x.saturate(5)
        return x
    return gen_recolor(func)

def gen_land_recolor2():
    def func(x):
        r, g, b = x.rgb
        if 3 * g / 2 > r + b:
            x = x.blend(spectra.rgb(0.7, 1, 0), ratio=0.05)
            x = x.saturate(10)
        return x
    return gen_recolor(func)


def remap_file(f_in, f_out, palmap):
    print(f"Converting {f_out}...")
    im = grf.open_image(f_in)
    data = np.array(im)
    data = palmap[data]
    im2 = Image.fromarray(data)
    im2.putpalette(im.getpalette())
    im2.save(f_out)


DEST_DIR = "gfx"

SOURCE_DIR = "/home/pavels/Builds/OpenGFX/sprites/png"
land_palmap = gen_land_recolor()
for fname in ("miscellaneous/hq.png",
              ):
    remap_file(os.path.join(SOURCE_DIR, fname), os.path.join(DEST_DIR, fname), land_palmap)


SOURCE_DIR = "/home/pavels/Projects/cmclient/local/ogfx-landscape-1.1.2-source/src/gfx"
land_palmap = gen_land_recolor()
for fname in ("grass_grid_temperate.gimp.png",
              "bare03_grid.gimp.png",
              "bare13_grid_temperate.gimp.png",
              "bare23_grid_temperate.gimp.png",
              "rough_grid_temperate.gimp.png",
              "rough_grid_temperate.gimp.png",
              "rocks_grid_temperate.gimp.png",
              "snow14_grid_alpine.gimp.png",
              "snow24_grid_alpine.gimp.png",
              "snow34_grid_alpine.gimp.png",
              "snow_grid.gimp.png",
              "water/seashore_grid_temperate.gimp.png",

              "infrastructure/tunnel_rail_grid_temperate.gimp.png",
              "infrastructure_road_tunnel_grid.png",
              ):
    remap_file(os.path.join(SOURCE_DIR, fname), os.path.join(DEST_DIR, fname), land_palmap)


land_palmap = gen_land_recolor2()
for fname in ("infrastructure_road_tunnel_grid.png",
              "infrastructure/road_grid_temperate.gimp.png",):
    remap_file(os.path.join(SOURCE_DIR, fname), os.path.join(DEST_DIR, fname), land_palmap)



def meadow_recolor(x):
    x = x.blend(spectra.rgb(0.7, 1, 0), ratio=0.2)
    return x.blend(spectra.rgb(1, 1, 0), ratio=0.4)

def half_meadow_recolor(x):
    x = x.blend(spectra.rgb(0.7, 1, 0), ratio=0.2)
    return x.blend(spectra.rgb(1, 1, 0), ratio=0.2)

# remap_file(os.path.join(SOURCE_DIR, "grass_grid_temperate.gimp.png"), os.path.join(DEST_DIR, "meadow_grid_temperate.png"), gen_recolor(meadow_recolor))
# remap_file(os.path.join(SOURCE_DIR, "grass_grid_temperate.gimp.png"), os.path.join(DEST_DIR, "half_meadow_grid_temperate.png"), gen_recolor(half_meadow_recolor))


# Generate snow transition tiles


grass = grf.open_image(os.path.join(SOURCE_DIR, "grass_grid_temperate.gimp.png"))
snow = grf.open_image(os.path.join(SOURCE_DIR, "snow_grid.gimp.png"))
grass_data = land_palmap[np.array(grass)]
snow_data = land_palmap[np.array(snow)]
for i in range(1, 4):
    ratio = [0, 0.15, 0.35, 0.7][i]
    grass_desaturate = [0, 10, 30, 15][i]
    grass_blend = [0, 0.1, 0.2, 0.2][i]
    print(f'Generating snow transitions {i}/3 ... ', end='')
    data = np.array(grass)
    cache = {(0, 0): 0}
    for y in range(grass.height):
        for x in range(grass.width):
            cache_key = grass_index, snow_index = snow_data[y, x], grass_data[y, x]
            if cache_key in cache:
                data[y, x] = cache[cache_key]
                continue
            snow_colour = grf.SPECTRA_PALETTE[snow_index]
            grass_colour = grf.SPECTRA_PALETTE[grass_index]
            grass_colour = grass_colour.blend(spectra.rgb(0.7, 0.7, 0), ratio=grass_blend)
            grass_colour = grass_colour.desaturate(grass_desaturate)
            data[y, x] = cache[cache_key] = grf.find_best_color(snow_colour.blend(grass_colour, ratio=ratio))
    im = Image.fromarray(data)
    im.putpalette(grf.PALETTE)
    im.save(os.path.join(DEST_DIR, f'snow_transition_{i}.png'))
    print('Done')



# Generate meadow transition tiles
im = Image.open(os.path.join(SOURCE_DIR, "grass_grid_temperate.gimp.png"))
din = np.array(im)

GROUND_SPRITES = [
                                        #  N E S W STEEP
    [   0 + 1,   1, 64, 31, -31,  0, 15, 15],  #
    [  80 + 1,   1, 64, 31, -31,  0,  7, 15],  #        W
    [ 160 + 1,   1, 64, 23, -31,  0, 15, 15],  #      S
    [ 240 + 1,   1, 64, 23, -31,  0,  7, 15],  #      S W
    [ 320 + 1,   1, 64, 31, -31,  0, 15,  7],  #    E
    [ 398 + 1,   1, 64, 31, -31,  0,  7,  7],  #    E   W
    [ 478 + 1,   1, 64, 23, -31,  0, 15,  7],  #    E S
    [ 558 + 1,   1, 64, 23, -31,  0,  7,  7],  #    E S W
    [ 638 + 1,   1, 64, 39, -31, -8, 23, 23],  #  N
    [ 718 + 1,   1, 64, 39, -31, -8, 15, 23],  #  N     W
    [ 798 + 1,   1, 64, 31, -31, -8, 23, 23],  #  N   S
    [ 878 + 1,   1, 64, 31, -31, -8, 15, 23],  #  N   S W
    [ 958 + 1,   1, 64, 39, -31, -8, 23, 15],  #  N E
    [1038 + 1,   1, 64, 39, -31, -8, 15, 15],  #  N E   W
    [1118 + 1,   1, 64, 31, -31, -8, 23, 15],  #  N E S
    [1196 + 1,   1, 64, 47, -31,-16, 23, 23],  #  N E   W STEEP
    [1276 + 1,   1, 64, 15, -31,  0,  7,  7],  #    E S W STEEP
    [1356 + 1,   1, 64, 31, -31, -8,  7, 23],  #  N   S W STEEP
    [1436 + 1,   1, 64, 31, -31, -8, 23,  7],  #  N E S   STEEP
]

# dout = np.zeros((64 * 8, 31 + 47 + 15), dtype=np.uint8)
# dout = np.zeros((31 + 47 + 15, 64 * 8), dtype=np.uint8)
# dout = np.zeros((64 * 16, im.width), dtype=np.uint8)
# for i in range (16):
#     print(f'Generating sprite row {i}/16...')
#     for j, (ox, oy, w, h, _ox, _oy, h1, h2) in enumerate(GROUND_SPRITES):
#         hn1 = 2. * (h1 + .5 - h / 2.) / h
#         hn2 = 2. * (h2 + .5 - h / 2.) / h
#         iflags = [i & (1 << k) for k in range(4)]
#         ut, uc, ub = i & 1, (i & 2) / 2, (i & 4) / 4
#         for y in range(0, h):
#             for x in range(0, w):
#                 c = din[y + oy, x + ox]
#                 if not c: continue
#                 xn = 2. * (x + .5 - w / 2.) / w
#                 yn = 2. * (y + .5 - h / 2.) / h
#                 dn = math.hypot(xn, yn + 1)
#                 ds = math.hypot(xn, yn - 1)
#                 de = math.hypot(xn - 1, yn - hn1)
#                 dw = math.hypot(xn + 1, yn - hn2)
#                 f = min(d if fl else 2. for d, fl in zip([dn, de, ds, dw], iflags))
#                 f = min(f * 1.44, 1.)
#                 bc = spectra.rgb(f, 1, 0)
#                 c = colors[c][1].blend(bc, ratio=0.2 + 0.1 * f)
#                 dout[oy + y + 64 * i, ox + x] = find_best_color(c)
# im2 = Image.fromarray(dout)
# im2.putpalette(im.getpalette())
# im2.save(os.path.join(DEST_DIR, "meadow_transitions.png"))


dmask = np.vectorize(lambda x: 0 if x and x != 0xFF else 0xFF)(din).astype('uint8')
immask = Image.fromarray(dmask, mode="L")
# immask.save(os.path.join(DEST_DIR, "mask.png"))

dout = np.zeros((64 * 81, im.width), dtype=np.uint8)
im2 = Image.fromarray(dout)
im2.putpalette(im.getpalette())
imd2 = ImageDraw.Draw(im2)
    # draw.line((0, 0) + im.size, fill=128)
    # draw.line((0, im.size[1], im.size[0], 0), fill=128)

def draw_bezier(imd, fill, width, a, b, c):
    N, M = 11, 2

    lerp = lambda x, y, t: t * x + (N - t) * y
    lerp2m = lambda a, b, t: (lerp(a[0], (a[0] + b[0]) // 2, t), lerp(a[1], (a[1] + b[1]) // 2, t))
    lerp2 = lambda a, b, t: (lerp(a[0], b[0], t), lerp(a[1], b[1], t))

    for t in range(-M, N + M + 1):
        p0 = lerp2m(a, b, t)
        p1 = lerp2m(c, b, N - t)
        pf = lerp2(p0, p1, t)
        x = pf[0] // N // N
        y = pf[1] // N // N
        imd.ellipse((x - width, y - width, x + width, y + width), fill=fill)


for i in range (81):
    print(f'Generating rivers sprite row {i + 1}/81...')

    points = (i % 3, (i // 3) % 3, (i // 9) % 3, (i // 27))
    inp = []
    outp = []
    for ii, p in enumerate(points):
        if p == 2: outp.append(ii)
        elif p == 1: inp.append(ii)

    if not inp: inp = [4]
    if not outp: outp = [4]

    for j, (ox, oy, w, h, _ox, _oy, h1, h2) in enumerate(GROUND_SPRITES):
        xx, yy = ox, oy + 64 * i

        # # tile outline
        # corners = ((0, h1), (w / 2, 0), (w, h2), (w / 2, h))
        # for ii in range(len(corners)):
        #     x1, y1 = corners[ii]
        #     x2, y2 = corners[(ii + 1) % len(corners)]
        #     imd2.line(((x1 + xx, y1 + yy), (x2 + xx, y2 + yy)), fill=0xA9, width=1)

        # hn1 = 2. * (h1 + .5 - h / 2.) / h
        # hn2 = 2. * (h2 + .5 - h / 2.) / h
        wc = w // 4
        edges = ((wc, h1 / 2), (w - wc, h2 / 2), (w - wc, (h + h2) / 2), (wc, (h + h1) / 2), (w / 2, h / 2))
        center = (w / 2  + xx, h / 2 + yy)
        # if not inp:

        #     continue
        for ii in inp:
            for oo in outp:
                xy = ((edges[ii][0] + xx, edges[ii][1] + yy), (edges[oo][0] + xx, edges[oo][1] + yy))
                draw_bezier(imd2, 0x42, 4, xy[0], center, xy[1])
        for ii in inp:
            for oo in outp:
                xy = ((edges[ii][0] + xx, edges[ii][1] + yy), (edges[oo][0] + xx, edges[oo][1] + yy))
                draw_bezier(imd2, 0xF5, 3, xy[0], center, xy[1])

        # # mark out
        # for oo in outp:
        #     x, y = edges[oo][0] + xx, edges[oo][1] + yy
        #     width = 3
        #     imd2.ellipse((x - width, y - width, x + width, y + width), fill=0x42)

    imd2.bitmap((0, i * 64), immask, fill=0)
        # iflags = [i & (1 << k) for k in range(4)]
        # ut, uc, ub = i & 1, (i & 2) / 2, (i & 4) / 4
        # for y in range(0, h):
        #     for x in range(0, w):
        #         c = din[y + oy, x + ox]
        #         if not c: continue
        #         xn = 2. * (x + .5 - w / 2.) / w
        #         yn = 2. * (y + .5 - h / 2.) / h
        #         dn = math.hypot(xn, yn + 1)
        #         ds = math.hypot(xn, yn - 1)
        #         de = math.hypot(xn - 1, yn - hn1)
        #         dw = math.hypot(xn + 1, yn - hn2)
        #         f = min(d if fl else 2. for d, fl in zip([dn, de, ds, dw], iflags))
        #         f = min(f * 1.44, 1.)
        #         bc = spectra.rgb(f, 1, 0)
        #         c = colors[c][1].blend(bc, ratio=0.2 + 0.1 * f)
        #         dout[oy + y + 64 * i, ox + x] = find_best_color(c)
# im2 = Image.fromarray(dout)
# im2.putpalette(im.getpalette())
im2.save(os.path.join(DEST_DIR, "rivers.png"))
