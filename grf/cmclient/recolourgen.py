import math
import spectra

# SAFE_COLORS = list(range(0xA0, 0xD8)) + [0xf5]
SAFE_COLORS = list(range(1, 0xD7))
f = open("../ttd-newgrf-dos.gpl")

while f.readline().strip() != "#":
    pass


def color_distance(c1, c2):
    rmean = (c1.rgb[0] + c2.rgb[0]) / 2.
    r = c1.rgb[0] - c2.rgb[0]
    g = c1.rgb[1] - c2.rgb[1]
    b = c1.rgb[2] - c2.rgb[2]
    return math.sqrt(
        ((2 + rmean) * r * r) +
        4 * g * g +
        (3 - rmean) * b * b)

# def color_distance(c1, c2):
#     r = c1[0] - c2[0]
#     g = c1[1] - c2[1]
#     b = c1[2] - c2[2]
#     return math.sqrt(r*r + g*g + b*b)

def gen_recolor(color_func):
    def func(x):
        x = color_func(x)
        mj, md = 0, 1e100
        for j, c in colors:
            if j not in SAFE_COLORS:
                continue
            d = color_distance(x, c)
            if d < md:
                mj, md = j, d
        return mj
    return func

def gen_tint(tint, ratio):
    return gen_recolor(lambda x: x.blend(tint, ratio=ratio))

def gen_brightness(level):
    def func(x):
        if level > 0:
            return x.brighten(amount=2.56 * level)
        else:
            return x.darken(amount=-2.56 * level)
    return gen_recolor(func)

def gen_white_tint_contrast():
    def func(x):
        white = spectra.rgb(1.0, 1.0, 1.0)
        x = x.blend(white, ratio=0.4)
        if color_distance(x, white) < 0.5:
            x = x.blend(spectra.rgb(0.5, 1.0, 0.0), ratio=0.2)
        return x
    return gen_recolor(func)

def gen_palette(func, comment):
    print(f"    // {comment}")
    print("    recolour_sprite {")
    print("        ", end="")
    for i, c in colors:
        mj = func(c)
        print(f"0x{i:02x}: 0x{mj:02x};", end=" ")
    print()
    print("    }")
    print()


colors = []
for _ in range(256):
    try:
        r, g, b, _, i = f.readline().split()
    except ValueError:
        break
    c = spectra.rgb(float(r) / 255., float(g) / 255., float(b) / 255.)
    # if c in SAFE_COLORS:
    colors.append((int(i), c))


for l in open("cmclient-header.nml"):
    print(l, end='')

# print("replace recolour_palettes(10389) {")
gen_palette(gen_tint(spectra.rgb(1, 0, 0), 0.6), "deep red tint")
gen_palette(gen_tint(spectra.rgb(1, 0.5, 0), 0.65), "deep orange tint")
gen_palette(gen_tint(spectra.rgb(0, 1, 0), 0.65), "deep green tint")
gen_palette(gen_tint(spectra.rgb(0, 1, 1), 0.65), "deep cyan tint")
gen_palette(gen_tint(spectra.rgb(1, 0, 0), 0.4), "red tint")
gen_palette(gen_tint(spectra.rgb(1, 0.5, 0), 0.4), "orange tint")
gen_palette(gen_tint(spectra.rgb(1.0, 1.0, 0), 0.4), "yellow tint")
gen_palette(gen_tint(spectra.rgb(1.0, 1.0, 0.5), 0.4), "yellow white tint")
# gen_palette(gen_tint(spectra.rgb(1.0, 1.0, 1.0), 0.4), "white tint")
gen_palette(gen_white_tint_contrast(), "white tint")
gen_palette(gen_tint(spectra.rgb(0, 1.0, 0), 0.4), "green tint")
gen_palette(gen_tint(spectra.rgb(0, 1.0, 1.0), 0.4), "cyan tint")
gen_palette(gen_tint(spectra.rgb(0.5, 1.0, 1.0), 0.4), "cyan white tint")
gen_palette(gen_tint(spectra.rgb(0, 0, 1.0), 0.4), "blue tint")

B = 22.2
gen_palette(gen_brightness(21.7 - B), "shade N")
gen_palette(gen_brightness(24.2 - B), "shade NE")  # 27.2
gen_palette(gen_brightness(25.7 - B), "shade E")  # 28.7
gen_palette(gen_brightness(23.4 - B), "shade SE")
gen_palette(gen_brightness(23.8 - B), "shade S")
gen_palette(gen_brightness(18.4 - B), "shade SW")
gen_palette(gen_brightness(17.1 - B), "shade W")
gen_palette(gen_brightness(17.5 - B), "shade NW")
print("}")
