import grf
import spectra

gen = grf.NewGRF(
    b'CMC\x01',
    'CityMania Client Resourse Pack v5',
    'Provides additional resources for a CityMania patched client (citymania.org/downloads). Should be put in your client data folder. Do not add this to your game via NewGRF options, it may break something.',
)

toolbar_png = grf.ImageFile('sprites/toolbar.png')
gen.add_sprite(grf.FileSprite(toolbar_png, 0, 0, 20, 14, yofs=4),
               grf.FileSprite(toolbar_png, 32, 0, 40, 40, zoom=grf.ZOOM_2X))

sprite = lambda *args, **kw: gen.add_sprite(grf.FileSprite(toolbar_png, *args, **kw))
sprite2 = lambda x, y, w, h, x2, y2: gen.add_sprite(grf.FileSprite(toolbar_png, x, y, w, h), grf.FileSprite(toolbar_png, x2, y2, w * 2, h * 2, zoom=grf.ZOOM_2X))
sprite( 0, 44, 12, 10)  # hq button icon
sprite( 0, 55, 12, 10)  # watch button icon
# sprite2(0, 55, 12, 10, 13, 55)  # watch button icon

sprite(45, 67, 10, 10)  # host black
sprite(45, 44, 10, 10)  # player black

sprite2(85, 67, 11, 11, 97, 67)  # host white
sprite2(85, 44, 11, 11, 97, 44)  # player white

sprite2( 0, 82, 11, 11,  0, 94)  # competitor coloured
sprite2(12, 82, 11, 11, 23, 94)  # host coloured
sprite2(24, 82, 11, 11, 46, 94)  # player coloured
sprite2(36, 82, 11, 11, 69, 94)  # company afk
sprite2(48, 82, 11, 11, 92, 94)  # company locked


innerhl_png = grf.ImageFile('sprites/innerhighlight00.png')
sprite = lambda *args, **kw: gen.add_sprite(grf.FileSprite(innerhl_png, *args, **kw))
sprite( 18,   8, 64, 31, xofs=-31, yofs= 7)
sprite( 98,   8, 64, 31, xofs=-31, yofs= 7)
sprite(178,   8, 64, 23, xofs=-31, yofs= 7)
sprite(258,   8, 64, 23, xofs=-31, yofs= 7)
sprite(338,   8, 64, 31, xofs=-31, yofs= 7)
sprite(418,   8, 64, 31, xofs=-31, yofs= 7)
sprite(498,   8, 64, 23, xofs=-31, yofs= 7)
sprite(578,   8, 64, 23, xofs=-31, yofs= 7)
sprite(658,   8, 64, 39, xofs=-31, yofs=-1)
sprite(  2,  72, 64, 39, xofs=-31, yofs=-1)
sprite( 82,  72, 64, 31, xofs=-31, yofs=-1)
sprite(162,  72, 64, 31, xofs=-31, yofs=-1)
sprite(242,  72, 64, 39, xofs=-31, yofs=-1)
sprite(322,  72, 64, 39, xofs=-31, yofs=-1)
sprite(402,  72, 64, 31, xofs=-31, yofs=-1)
sprite(482,  72, 64, 47, xofs=-31, yofs=-9)
sprite(562,  72, 64, 15, xofs=-31, yofs= 7)
sprite(642,  72, 64, 31, xofs=-31, yofs=-1)
sprite(722,  72, 64, 31, xofs=-31, yofs=-1)
sprite(  2, 136, 20, 14, xofs=  1, yofs= 5)
sprite( 34, 136, 20, 20, xofs=  0, yofs= 0)
sprite(185, 125, 18, 15, xofs= -8, yofs= 7)

#red
gen.add_sprite(grf.PaletteRemap([
    [0x01, 0x09, 0x00],
    [0x0A, 0x0B, 0xB6],
    [0x0C, 0x0D, 0xB7],
    [0x0E, 0x0F, 0xB8],
    [0x10, 0xFF, 0x00],
]))

# green
gen.add_sprite(grf.PaletteRemap([
    [0x0A, 0x0B, 0xCF],
    [0x0C, 0x0D, 0xD0],
    [0x0E, 0x0F, 0xD1],
]))

# black
gen.add_sprite(grf.PaletteRemap([
    [0x0A, 0x0B, 0x10],
    [0x0C, 0x0D, 0x11],
    [0x0E, 0x0F, 0x12],
]))

# ligth blue
gen.add_sprite(grf.PaletteRemap([
    [0x0A, 0x0B, 0x96],
    [0x0C, 0x0D, 0x97],
    [0x0E, 0x0F, 0x98],
]))

# orange
gen.add_sprite(grf.PaletteRemap([
    [0x0A, 0x0B, 0xB9],
    [0x0C, 0x0D, 0xBA],
    [0x0E, 0x0F, 0xBB],
]))

# white
gen.add_sprite(grf.PaletteRemap([
    [0x0A, 0x0B, 0x0D],
    [0x0C, 0x0D, 0x0E],
    [0x0E, 0x0F, 0x0F],
]))

# white (yellow)
gen.add_sprite(grf.PaletteRemap([
    [0x0A, 0x0B, 0x32],
    [0x0C, 0x0D, 0x33],
    [0x0E, 0x0F, 0x34],
]))

# white (purple)
gen.add_sprite(grf.PaletteRemap([
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
    for _ in range(1, 16 + 4):
        gen.add_sprite(grf.FileSprite(borderhl_png, x, y, w, h, xofs=xofs, yofs=yofs))
        x += SPRITE_MARGIN + w

    y += h + SPRITE_MARGIN


def gen_tint(tint, ratio):
    return lambda x: x.blend(tint, ratio=ratio)

def gen_brightness(level):
    def func(x):
        if level > 0:
            return x.brighten(amount=2.56 * level)
        else:
            return x.darken(amount=-2.56 * level)
    return func

def gen_white_tint_contrast():
    def func(x):
        white = spectra.rgb(1.0, 1.0, 1.0)
        x = x.blend(white, ratio=0.4)
        if grf.color_distance(x, white) < 0.5:
            x = x.blend(spectra.rgb(0.5, 1.0, 0.0), ratio=0.2)
        return x
    return func


remap = lambda f: gen.add_sprite(grf.PaletteRemap.from_function(f, remap_water=True))
remap(gen_tint(spectra.rgb(1, 0, 0), 0.6))  # deep red tint
remap(gen_tint(spectra.rgb(1, 0.5, 0), 0.65))  # deep orange tint
remap(gen_tint(spectra.rgb(0, 1, 0), 0.65))  # deep green tint
remap(gen_tint(spectra.rgb(0, 1, 1), 0.65))  # deep cyan tint
remap(gen_tint(spectra.rgb(1, 0, 0), 0.4))  # red tint
remap(gen_tint(spectra.rgb(1, 0.5, 0), 0.4))  # orange tint
remap(gen_tint(spectra.rgb(1.0, 1.0, 0), 0.4))  # yellow tint
remap(gen_tint(spectra.rgb(1.0, 1.0, 0.5), 0.4))  # yellow white tint
remap(gen_white_tint_contrast())  # white tint
remap(gen_tint(spectra.rgb(0, 1.0, 0), 0.4))  # green tint
remap(gen_tint(spectra.rgb(0, 1.0, 1.0), 0.4))  # cyan tint
remap(gen_tint(spectra.rgb(0.5, 1.0, 1.0), 0.4))  # cyan white tint
remap(gen_tint(spectra.rgb(0, 0, 1.0), 0.4))  # blue tint

B = 22.2
remap(gen_brightness(21.7 - B))  # shade N
remap(gen_brightness(24.2 - B))  # shade NE  27.2
remap(gen_brightness(25.7 - B))  # shade E   28.7
remap(gen_brightness(23.4 - B))  # shade SE
remap(gen_brightness(23.8 - B))  # shade S
remap(gen_brightness(18.4 - B))  # shade SW
remap(gen_brightness(17.1 - B))  # shade W
remap(gen_brightness(17.5 - B))  # shade NW

gen.write('../../bin/data/cmclient-5.grf')
