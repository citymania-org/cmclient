import grf

import numpy as np


g = grf.BaseNewGRF()
g.add(grf.SetDescription(
    format_version=8,
    grfid=b'CMC\x01',
    name='CityMania Client Resourse Pack v5',
    description='Provides additional resources for a CityMania patched client (citymania.org/downloads). Should be put in your client data folder. Do not add this to your game via NewGRF options, it may break something.',
))

toolbar_png = grf.ImageFile('sprites/toolbar.png')
g.add(grf.AlternativeSprites(
    grf.FileSprite(toolbar_png, 0, 0, 20, 14, yofs=4),
    grf.FileSprite(toolbar_png, 32, 0, 40, 40, zoom=grf.ZOOM_2X)
))

sprite = lambda name, *args, **kw: g.add(grf.FileSprite(toolbar_png, *args, name=name, **kw))
sprite2 = lambda name, x, y, w, h, x2, y2: g.add(grf.AlternativeSprites(grf.FileSprite(toolbar_png, x, y, w, h, name=name), grf.FileSprite(toolbar_png, x2, y2, w * 2, h * 2, name=name + '_2x', zoom=grf.ZOOM_2X)))
sprite('hq_icon', 0, 44, 12, 10)  # hq button icon
sprite('watch_icon', 0, 55, 12, 10)  # watch button icon
# sprite2(0, 55, 12, 10, 13, 55)  # watch button icon

sprite('host_black', 45, 67, 10, 10)  # host black
sprite('player_black', 45, 44, 10, 10)  # player black

sprite2('host_white', 85, 67, 11, 11, 97, 67)  # host white
sprite2('player_white', 85, 44, 11, 11, 97, 44)  # player white

sprite2('competitor_icon', 0, 82, 11, 11,  0, 94)  # competitor coloured
sprite2('host_icon', 12, 82, 11, 11, 23, 94)  # host coloured
sprite2('player_icon', 24, 82, 11, 11, 46, 94)  # player coloured
sprite2('company_afk_icon', 36, 82, 11, 11, 69, 94)  # company afk
sprite2('company_locked_icon', 48, 82, 11, 11, 92, 94)  # company locked


innerhl_png = grf.ImageFile('sprites/innerhighlight00.png')
sprite = lambda name, *args, **kw: g.add(grf.FileSprite(innerhl_png, *args, name=name, **kw))
sprite('inner_hl',  18,   8, 64, 31, xofs=-31, yofs= 7)
sprite('inner_hl',  98,   8, 64, 31, xofs=-31, yofs= 7)
sprite('inner_hl', 178,   8, 64, 23, xofs=-31, yofs= 7)
sprite('inner_hl', 258,   8, 64, 23, xofs=-31, yofs= 7)
sprite('inner_hl', 338,   8, 64, 31, xofs=-31, yofs= 7)
sprite('inner_hl', 418,   8, 64, 31, xofs=-31, yofs= 7)
sprite('inner_hl', 498,   8, 64, 23, xofs=-31, yofs= 7)
sprite('inner_hl', 578,   8, 64, 23, xofs=-31, yofs= 7)
sprite('inner_hl', 658,   8, 64, 39, xofs=-31, yofs=-1)
sprite('inner_hl',   2,  72, 64, 39, xofs=-31, yofs=-1)
sprite('inner_hl',  82,  72, 64, 31, xofs=-31, yofs=-1)
sprite('inner_hl', 162,  72, 64, 31, xofs=-31, yofs=-1)
sprite('inner_hl', 242,  72, 64, 39, xofs=-31, yofs=-1)
sprite('inner_hl', 322,  72, 64, 39, xofs=-31, yofs=-1)
sprite('inner_hl', 402,  72, 64, 31, xofs=-31, yofs=-1)
sprite('inner_hl', 482,  72, 64, 47, xofs=-31, yofs=-9)
sprite('inner_hl', 562,  72, 64, 15, xofs=-31, yofs= 7)
sprite('inner_hl', 642,  72, 64, 31, xofs=-31, yofs=-1)
sprite('inner_hl', 722,  72, 64, 31, xofs=-31, yofs=-1)
sprite('inner_hl',   2, 136, 20, 14, xofs=  1, yofs= 5)
sprite('inner_hl',  34, 136, 20, 20, xofs=  0, yofs= 0)
sprite('inner_hl', 185, 125, 18, 15, xofs= -8, yofs= 7)

#red
g.add(grf.PaletteRemap([
    [0x01, 0x09, 0x00],
    [0x0A, 0x0B, 0xB6],
    [0x0C, 0x0D, 0xB7],
    [0x0E, 0x0F, 0xB8],
    [0x10, 0xFF, 0x00],
]))

# green
g.add(grf.PaletteRemap([
    [0x0A, 0x0B, 0xCF],
    [0x0C, 0x0D, 0xD0],
    [0x0E, 0x0F, 0xD1],
]))

# black
g.add(grf.PaletteRemap([
    [0x0A, 0x0B, 0x10],
    [0x0C, 0x0D, 0x11],
    [0x0E, 0x0F, 0x12],
]))

# ligth blue
g.add(grf.PaletteRemap([
    [0x0A, 0x0B, 0x96],
    [0x0C, 0x0D, 0x97],
    [0x0E, 0x0F, 0x98],
]))

# orange
g.add(grf.PaletteRemap([
    [0x0A, 0x0B, 0xB9],
    [0x0C, 0x0D, 0xBA],
    [0x0E, 0x0F, 0xBB],
]))

# white
g.add(grf.PaletteRemap([
    [0x0A, 0x0B, 0x0D],
    [0x0C, 0x0D, 0x0E],
    [0x0E, 0x0F, 0x0F],
]))

# white (yellow)
g.add(grf.PaletteRemap([
    [0x0A, 0x0B, 0x32],
    [0x0C, 0x0D, 0x33],
    [0x0E, 0x0F, 0x34],
]))

# white (purple)
g.add(grf.PaletteRemap([
    [0x0A, 0x0B, 0xAC],
    [0x0C, 0x0D, 0xAD],
    [0x0E, 0x0F, 0xAE],
]))

borderhl_png = grf.ImageFile('sprites/borderhighlight.png')
SPRITE_MARGIN = 10
TILEDATA = [
    [  20,   20,  64,  31, -31,   7],
    [  20,   70,  64,  31, -31,   7],
    [  20,  120,  64,  23, -31,   7],
    [  20,  170,  64,  23, -31,   7],
    [  20,  220,  64,  31, -31,   7],
    [  20,  270,  64,  31, -31,   7],
    [  20,  320,  64,  23, -31,   7],
    [  20,  370,  64,  23, -31,   7],
    [  20,  420,  64,  39, -31,  -1],
    [  20,  470,  64,  39, -31,  -1],
    [  20,  520,  64,  31, -31,  -1],
    [  20,  570,  64,  31, -31,  -1],
    [  20,  620,  64,  39, -31,  -1],
    [  20,  670,  64,  39, -31,  -1],
    [  20,  720,  64,  31, -31,  -1],
    [  20,  770,  64,  47, -31,  -9],
    [  20,  820,  64,  15, -31,   7],
    [  20,  870,  64,  31, -31,  -1],
    [  20,  920,  64,  31, -31,  -1],
]
y = SPRITE_MARGIN
for p in TILEDATA:
    x = SPRITE_MARGIN
    _, _, w, h, xofs, yofs = p
    for i in range(1, 16 + 4):
        g.add(grf.FileSprite(borderhl_png, x, y, w, h, xofs=xofs, yofs=yofs, name=f'border_hl_{i}'))
        x += SPRITE_MARGIN + w

    y += h + SPRITE_MARGIN


def gen_tint(tint, ratio):
    tint = grf.srgb_to_oklab(np.array(tint) * 255)
    return lambda x: grf.oklab_blend(x, tint, ratio=1.-(1 - ratio)**2)


def gen_brightness(level):
    def func(x):
        if level > 0:
            return grf.oklab_blend(x, grf.srgb_to_oklab((255, 255, 192)), ratio=0.04 * level)
        else:
            return grf.oklab_blend(x, grf.srgb_to_oklab((4, 0, 16)), ratio=-0.04 * level)
        # l, a, b = x
        # l = min(max(l + 0.019 * level, 0.), 1.)
        # return np.array((l, a, b))
    return func


def gen_white_tint_contrast():
    def func(x):
        # white = grf.srgb_to_oklab((255, 255, 255))
        white = np.array((1., 0, 0))
        x = grf.oklab_blend(x, white, ratio=0.4)
        # if grf.color_distance(x, white) < 0.5:
        #     x = x.blend(spectra.rgb(0.5, 1.0, 0.0), ratio=0.2)
        return x
    return func


def gen_oklab_tint(tint, ratio):
    return lambda x: grf.oklab_blend(x, np.array(tint), ratio=ratio)


# (0.5498, 0.17, 0.1)
# (0.7433, 0.09, 0.15)
# (0.7433, 0, 0.15)
# (0.7418, -0.09, 0.15)

remap = lambda f: g.add(grf.PaletteRemap.oklab_from_function(f, remap_range=grf.ALL_COLOURS))

# Tree shades
B = 22.2
remap(gen_brightness(21.7 - B))  # shade N
remap(gen_brightness(24.2 - B))  # shade NE  27.2
remap(gen_brightness(25.7 - B))  # shade E   28.7
remap(gen_brightness(23.4 - B))  # shade SE
remap(gen_brightness(23.8 - B))  # shade S
remap(gen_brightness(18.4 - B))  # shade SW
remap(gen_brightness(17.1 - B))  # shade W
remap(gen_brightness(17.5 - B))  # shade NW

BASE_TINTS = {
    'red_deep': gen_tint((1, 0, 0), 0.6),
    'orange_deep': gen_tint((1, 0.5, 0), 0.65),
    #'green_deep': gen_tint((0, 1, 0), 0.65),
    #'cyan_deep': gen_tint((0, 1, 1), 0.65),
    'red': gen_tint((1, 0, 0), 0.4),
    'orange': gen_tint((1, 0.5, 0), 0.4),
    'yellow': gen_tint((1.0, 1.0, 0), 0.4),
    'white': gen_white_tint_contrast(),
    'green': gen_tint((0, 1.0, 0), 0.4),
    'cyan': gen_tint((0, 1.0, 1.0), 0.4),
    'blue': gen_tint((0, 0, 1.0), 0.4),
}
# remap(gen_tint((0.5, 1.0, 1.0), 0.4))  # cyan white tint

for f in BASE_TINTS.values():
    remap(f)

for f1 in BASE_TINTS.values():
    for f2 in BASE_TINTS.values():
        remap(lambda x: f2(f1(x)))

# Only white can be mixed over any combination
white = BASE_TINTS['white']
for f1 in BASE_TINTS.values():
    for f2 in BASE_TINTS.values():
        remap(lambda x: white(f2(f1(x))))

grf.main(g, '../../bin/data/cmclient-6.grf')

# func = gen_brightness(17.1 - B)
# grf.make_palette_image([grf.oklab_to_srgb(func(x)) for x in grf.OKLAB_PALETTE]).show()
# remap = grf.PaletteRemap.oklab_from_function(func, remap_water=True).remap
# grf.make_palette_image([grf.PALETTE[x] for x in remap]).show()
