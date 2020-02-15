import math
import spectra

# SAFE_COLORS = list(range(0xA0, 0xD8)) + [0xf5]
SAFE_COLORS = list(range(1, 0xD7))
# f = open("ttd-noaction-nocomp.gpl")
f = open("ttd-newgrf-dos.gpl")

while f.readline().strip() != "#":
    pass


# def color_distance(c1, c2):
#     rmean = (c1[0] + c2[0]) / 2.
#     r = c1[0] - c2[0]
#     g = c1[1] - c2[1]
#     b = c1[2] - c2[2]
#     return math.sqrt(
#         (((512 + rmean) * r * r) / 256.) +
#         4 * g * g +
#         (((767 - rmean) * b * b) / 256.))

def color_distance(c1, c2):
    r = c1[0] - c2[0]
    g = c1[1] - c2[1]
    b = c1[2] - c2[2]
    return math.sqrt(r*r + g*g + b*b)


def gen_recolor(tint, ratio, comment):
    print(f"    // {comment}")
    print("    recolour_sprite {")
    print("        ", end="")
    for i, (r, g, b) in colors:
        x = spectra.rgb(r / 255., g / 255., b / 255.)
        x = x.blend(tint, ratio=ratio)
        r, g, b = [int(q * 255) for q in x.clamped_rgb]
        mj, md = 0, 1e100
        for j, c in colors:
            if j not in SAFE_COLORS:
                continue
            d = color_distance((r, g, b), c)
            if d < md:
                mj, md = j, d
            # mj = 0xa4
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
    c = (int(r), int(g), int(b))
    # if c in SAFE_COLORS:
    colors.append((int(i), c))

gen_recolor(spectra.rgb(1, 0, 0), 0.6, "deep red tint")
gen_recolor(spectra.rgb(1, 0.5, 0), 0.65, "deep orange tint")
gen_recolor(spectra.rgb(0, 1, 0), 0.65, "deep green tint")
gen_recolor(spectra.rgb(0, 1, 1), 0.65, "deep cyan tint")
gen_recolor(spectra.rgb(1, 0, 0), 0.4, "red tint")
gen_recolor(spectra.rgb(1, 0.5, 0), 0.4, "orange tint")
gen_recolor(spectra.rgb(1.0, 1.0, 0), 0.4, "yellow tint")
gen_recolor(spectra.rgb(1.0, 1.0, 0.5), 0.4, "yellow white tint")
gen_recolor(spectra.rgb(1.0, 1.0, 1.0), 0.4, "white tint")
gen_recolor(spectra.rgb(0, 1.0, 0), 0.4, "green tint")
gen_recolor(spectra.rgb(0, 1.0, 1.0), 0.4, "cyan tint")
gen_recolor(spectra.rgb(0.5, 1.0, 1.0), 0.4, "cyan white tint")
gen_recolor(spectra.rgb(0, 0, 1.0), 0.4, "blue tint")
