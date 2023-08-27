from pathlib import Path

import numpy as np
import spectra
from PIL import Image

import grf


OGFX_PATH = Path('OpenGFX2_Classic-0.1')
OGFX_BASE_PATH = OGFX_PATH / 'ogfx21_base_8.grf'
THIS_FILE = grf.PythonFile(__file__)


class GRFFile(grf.LoadedResourceFile):
    def __init__(self, path):
        self.path = path
        self.real_sprites = None
        self.load()

    def load(self):
        if self.real_sprites is not None:
            return
        print(f'Decompiling {self.path}...')
        self.context = grf.decompile.ParsingContext()
        self.f = open(self.path, 'rb')
        self.gen, self.container, self.real_sprites = grf.decompile.read(self.f, self.context)

    def unload(self):
        self.context = None
        self.gen = None
        self.container = None
        self.real_sprites = None
        self.f.close()

    def get_sprite_data(self, sprite_id):
        s = self.real_sprites[sprite_id + 1]
        assert len(s) == 1, len(s)
        s = s[0]
        self.f.seek(s.offset)
        data, _ = grf.decompile.decode_sprite(self.f, s, self.container)
        return data

    def get_sprite(self, sprite_id):
        s = self.real_sprites[sprite_id + 1]
        assert len(s) == 1, len(s)
        s = s[0]
        return GRFSprite(self, s.type, s.bpp, sprite_id, w=s.width, h=s.height, xofs=s.xofs, yofs=s.yofs, zoom=s.zoom, crop=False)


class GRFSprite(grf.Sprite):

    class Mask(grf.ImageMask):
        def get_fingerprint(self):
            return True

    def __init__(self, file, type, grf_bpp, sprite_id, *args, **kw):
        super().__init__(*args, **kw)
        self.file = file
        self.type = type
        self.grf_bpp = grf_bpp
        self.sprite_id = sprite_id

        bpp = grf_bpp
        if self.type & 0x04:
            bpp -= 1

        if bpp == 3:
            self.bpp = grf.BPP_24
        elif bpp == 4:
            self.bpp = grf.BPP_32
        else:
            self.bpp = grf.BPP_8

        self._image = None

    def get_resource_files(self):
        return (self.file, THIS_FILE)

    def get_fingerprint(self):
        return {
            'class': self.__class__.__name__,
            'sprite_id': self.sprite_id,
        }

    def load(self):
        if self._image is not None:
            return

        data = self.file.get_sprite_data(self.sprite_id)

        a = np.frombuffer(data, dtype=np.uint8)
        assert a.size == self.w * self.h * self.grf_bpp, (a.size, self.w, self.h, self.grf_bpp)

        bpp = self.grf_bpp
        a.shape = (self.h, self.w, bpp)

        mask = None
        if self.type & 0x04 > 0:
            bpp -= 1
            mask = Image.fromarray(a[:, :, -1], mode='P')
            mask.putpalette(grf.PALETTE)

        if bpp == 3:
            self._image = Image.fromarray(a[:, :, :bpp], mode='RGB'), grf.BPP_24
        elif bpp == 4:
            self._image = Image.fromarray(a[:, :, :bpp], mode='RGBA'), grf.BPP_32
        else:
            self._image = mask, grf.BPP_8
            mask = None

        if mask is not None:
            self.mask = self.Mask(mask)

        # return self._image[0].show()

    def get_image(self):
        self.load()
        return self._image


class RecolourSprite(grf.Sprite):
    def __init__(self, sprite, recolour):
        super().__init__(sprite.w, sprite.h, xofs=sprite.xofs, yofs=sprite.yofs, bpp=sprite.bpp, zoom=sprite.zoom, crop=sprite.crop)
        self.sprite = sprite
        self.recolour = recolour
        self._image = None

    def get_resource_files(self):
        return self.sprite.get_resource_files()

    def get_fingerprint(self):
        return grf.combine_fingerprint(
            super().get_fingerprint_base(),
            sprite=self.sprite.get_fingerprint,
            recolour=True,
        )

    def get_image(self):
        if self._image is not None:
            return self._image
        im, bpp = self.sprite.get_image()
        assert bpp == 8
        data = np.array(im)
        data = self.recolour[data]
        im2 = Image.fromarray(data)
        im2.putpalette(im.getpalette())
        self._image = im2, grf.BPP_8
        return self._image


class SnowTransitionSprite(grf.Sprite):
    def __init__(self, grass_sprite, snow_sprite, snow_level):
        assert snow_level in (1, 2, 3)
        self.grass_sprite = grass_sprite
        self.snow_sprite = snow_sprite
        self.snow_level = snow_level
        super().__init__(
            grass_sprite.w,
            grass_sprite.h,
            xofs=grass_sprite.xofs,
            yofs=grass_sprite.yofs,
            zoom=grass_sprite.zoom,
            crop=grass_sprite.crop,
        )
        self._image = None

    def get_image(self):
        if self._image is not None:
            return self._image

        grass_data = np.array(self.grass_sprite.get_image()[0])
        snow_data = np.array(self.snow_sprite.get_image()[0])

        ratio = [0, 0.15, 0.35, 0.7][self.snow_level]
        grass_desaturate = [0, 10, 30, 15][self.snow_level]
        grass_blend = [0, 0.1, 0.2, 0.2][self.snow_level]

        data = grass_data.copy()
        cache = {(0, 0): 0}
        h, w = data.shape
        for y in range(h):
            for x in range(w):
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
        self._image = im, grf.BPP_8
        return self._image

    def get_resource_files(self):
        return self.grass_sprite.get_resource_files() + self.snow_sprite.get_resource_files()

    def get_fingerprint(self):
        return grf.combine_fingerprint(
            grass=self.grass_sprite.get_fingerprint,
            snow=self.snow_sprite.get_fingerprint,
            level=self.snow_level,
        )


def gen_recolour(color_func):
    out = np.arange(256, dtype=np.uint8)
    for i, c in grf.SPECTRA_PALETTE.items():
        if i not in grf.SAFE_COLOURS:
            continue
        out[i] = grf.find_best_color(color_func(c))
    return out


def gen_tint(tint, ratio):
    return lambda x: grf.find_best_color(x.blend(tint, ratio=ratio))


def gen_brightness(level):
    def func(x):
        if level > 0:
            return x.brighten(amount=2.56 * level)
        else:
            return x.darken(amount=-2.56 * level)
    return gen_recolour(func)


def gen_land_recolour():
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
    return gen_recolour(func)


def gen_land_recolour2():
    def func(x):
        r, g, b = x.rgb
        if 3 * g / 2 > r + b:
            x = x.blend(spectra.rgb(0.7, 1, 0), ratio=0.05)
            x = x.saturate(10)
        return x
    return gen_recolour(func)


GROUND_RECOLOUR = gen_land_recolour()

g = grf.NewGRF(
    grfid=b'CMAL',
    name='CityMania Alpine Landscape for OpenGFX2',
    description='Modified OpenGFX2 sprites for alpine climate.',
    version=3,
    min_compatible_version=0,
)
ogfx = GRFFile(OGFX_BASE_PATH)


def replace_old_sprites(first_id, amount, sprite_func):
    g.add(grf.ReplaceOldSprites([(first_id, amount)]))
    res = []
    for i in range(amount):
        res.append(sprite_func(first_id, i))
    g.add(*res)
    return res


# shore sprites (replacing 16 seems to do these as well)
# replace_shore_sprites(4062, 'gfx/water/seashore_grid_temperate.gimp.png', 1, 1)
# def replace_coastal_sprites(file, x, y, **kw):
#     png = grf.ImageFile(file)
#     gen.add(grf.ReplaceNewSprites(0x0d, 16))
#     sprite = lambda *args, **kw: gen.add(grf.FileSprite(png, *args, **kw))
#     sprite(1276+x,   y, 64, 15, xofs=-31, yofs=  0, **kw)
#     sprite(  80+x,   y, 64, 31, xofs=-31, yofs=  0, **kw)
#     sprite( 160+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)
#     sprite( 240+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)
#     sprite( 320+x,   y, 64, 31, xofs=-31, yofs=  0, **kw)
#     sprite(1356+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)
#     sprite( 478+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)
#     sprite( 558+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)
#     sprite( 638+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)
#     sprite( 718+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)
#     sprite(1196+x,   y, 64, 47, xofs=-31, yofs=-16, **kw)
#     sprite( 878+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)
#     sprite( 958+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)
#     sprite(1038+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)
#     sprite(1118+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)
#     sprite(1436+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)


# replace_coastal_sprites('gfx/water/seashore_grid_temperate.gimp.png', 1, 1)


def ground_func(first_id, i):
    ogfx_sprite = ogfx.get_sprite(first_id + i)
    return RecolourSprite(ogfx_sprite, GROUND_RECOLOUR)


# Normal land
grass = replace_old_sprites(3981, 19, ground_func)

#bulldozed (bare) land and regeneration stages:
replace_old_sprites(3924, 19, ground_func)
replace_old_sprites(3943, 19, ground_func)
replace_old_sprites(3962, 19, ground_func)

# rough terrain
replace_old_sprites(4000, 19 + 4, ground_func)

# rocky terrain
replace_old_sprites(4023, 19, ground_func)

# road sprites
replace_old_sprites(1332, 19, ground_func)
replace_old_sprites(2389, 8, ground_func)  # tunnels

# rail tunnels
replace_old_sprites(2365, 8, ground_func)

# hq
replace_old_sprites(2603, 29, ground_func)

# different snow densities:
snow = replace_old_sprites(4550, 19, ground_func)
replace_old_sprites(4493, 19, lambda _, i: SnowTransitionSprite(grass[i], snow[i], 1))
replace_old_sprites(4512, 19, lambda _, i: SnowTransitionSprite(grass[i], snow[i], 2))
replace_old_sprites(4531, 19, lambda _, i: SnowTransitionSprite(grass[i], snow[i], 3))


# CREEKS

def tmpl_ground_sprites(func, x, y, **kw):
    func(   0+x,   y, 64, 31, xofs=-31, yofs=  0, **kw)  #
    func(  80+x,   y, 64, 31, xofs=-31, yofs=  0, **kw)  #       W
    func( 160+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)  #     S
    func( 240+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)  #     S W
    func( 320+x,   y, 64, 31, xofs=-31, yofs=  0, **kw)  #   E
    func( 398+x,   y, 64, 31, xofs=-31, yofs=  0, **kw)  #   E   W
    func( 478+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)  #   E S
    func( 558+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)  #   E S W
    func( 638+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)  # N
    func( 718+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)  # N     W
    func( 798+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)  # N   S
    func( 878+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)  # N   S W
    func( 958+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)  # N E
    func(1038+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)  # N E   W
    func(1118+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)  # N E S
    func(1196+x,   y, 64, 47, xofs=-31, yofs=-16, **kw)  # N E   W STEEP
    func(1276+x,   y, 64, 15, xofs=-31, yofs=  0, **kw)  #   E S W STEEP
    func(1356+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)  # N   S W STEEP
    func(1436+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)  # N E S   STEEP


# Tile slope to sprite offset
get_tile_slope_offset = grf.Switch(
    feature=grf.OBJECT,
    ref_id=0,
    ranges={0: 0, 1: 1, 2: 2, 4: 4, 8: 8, 9: 9, 3: 3, 6: 6, 12: 12, 5: 5, 10: 10, 11: 11, 7: 7, 14: 14, 13: 13, 27: 17, 23: 16, 30: 18, 29: 15},
    default=0,
    code='tile_slope'
)
g.add(get_tile_slope_offset)

# Ground sprite
get_ground_sprite = grf.Switch(
    feature=grf.OBJECT,
    ref_id=1,
    ranges={-2: 4550 , -1: 4550 - 19, 0: 4550 - 19 * 2, 1: 4550 - 19 * 3},
    default=3981,
    code='max(snowline_height - tile_height, -2)'
)
g.add(get_ground_sprite)

g.add(grf.Action1(grf.OBJECT, 81, 19))
png = grf.ImageFile("gfx/rivers.png")
for i in range(81):
    tmpl_ground_sprites(lambda *args, **kw: g.add(grf.FileSprite(png, *args, **kw)), 1, i * 64 + 1)

for i in range(81):
    g.add(layout := grf.AdvancedSpriteLayout(
        feature=grf.OBJECT,
        ground={
            'sprite': grf.SpriteRef(0, is_global=True),
            'flags': 2,
            'add': grf.Temp(1),
        },
        buildings=[{
            'sprite': grf.SpriteRef(i, is_global=False),
            'flags': 2,
            'add': grf.Temp(0),
        }]
    ))

    g.add(layout_switch := grf.Switch(
        ranges={0: layout},
        default=layout,
        code=f'''
            TEMP[0] = call({get_tile_slope_offset})
            TEMP[1] = call({get_ground_sprite}) + TEMP[0]
        '''
    ))
    g.add(creek_obj := grf.Define(
        feature=grf.OBJECT,
        id=i,
        props={
            'class' : b'CREE',
            'size' : (1, 1),
            'climates_available' : grf.ALL_CLIMATES,
            'end_of_life_date' : 0,
            'flags' : grf.Object.Flags.HAS_NO_FOUNDATION | grf.Object.Flags.ALLOW_UNDER_BRIDGE | grf.Object.Flags.AUTOREMOVE,
        }
    ))


    g.add(grf.Map(
        definition=creek_obj,
        maps={255: layout_switch},
        default=layout_switch,
    ))

grf.main(g, 'alpine.grf')
