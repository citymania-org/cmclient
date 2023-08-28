from pathlib import Path

import numpy as np
import spectra
from PIL import Image

import grf

OGFX_PATH = Path('opengfx-0.5.2')
OGFX_BASE_PATH = OGFX_PATH / 'ogfx1_base.grf'
OGFX_EXTRA_BASE_PATH = OGFX_PATH / 'ogfxe_extra.grf'
THIS_FILE = grf.PythonFile(__file__)

# OGFX_BASE_PATH = Path('OpenGFX_BigGUI-2.0.0/ogfx-biggui.grf')

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
        self.g, self.container, self.real_sprites = grf.decompile.read(self.f, self.context)

    def unload(self):
        self.context = None
        self.gen = None
        self.container = None
        self.real_sprites = None
        self.f.close()

    def get_sprite_data(self, sprite_id, num):
        s = self.real_sprites[sprite_id + 1][num]
        self.f.seek(s.offset)
        data, _ = grf.decompile.decode_sprite(self.f, s, self.container)
        return data

    def get_sprites(self, sprite_id):
        return [
            GRFSprite(self, s.type, s.bpp, sprite_id, i, w=s.width, h=s.height, xofs=s.xofs, yofs=s.yofs, zoom=s.zoom, crop=False)
            for i, s in enumerate(self.real_sprites[sprite_id + 1])
        ]


class GRFSprite(grf.Sprite):

    class Mask(grf.ImageMask):
        def get_fingerprint(self):
            return True

    def __init__(self, file, type, grf_bpp, sprite_id, num, *args, **kw):
        super().__init__(*args, **kw)
        self.file = file
        self.type = type
        self.grf_bpp = grf_bpp
        self.sprite_id = sprite_id
        self.num = num  # index with the same sprite_id

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

        data = self.file.get_sprite_data(self.sprite_id, self.num)

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


def gen_recolour(color_func):
    out = np.arange(256, dtype=np.uint8)
    for i, c in grf.SPECTRA_PALETTE.items():
        if i not in grf.SAFE_COLOURS:
            continue
        out[i] = grf.find_best_color(color_func(c))
    return out


def gen_tint(tint, ratio):
    return lambda x: x.blend(tint, ratio=ratio)

GREEN = gen_recolour(gen_tint(spectra.rgb(0, 1, 0), 0.5))


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


g = grf.NewGRF(
    grfid=b'CMU1',
    name='OpenGFX1 GUI sprites',
    description='Only GUI sprites from OpenGFX1. For those like me who don''t want to wait for baseset parameters',
    version=0,
    min_compatible_version=0,
)

ogfx = GRFFile(OGFX_BASE_PATH)
ogfx_extra = GRFFile(OGFX_EXTRA_BASE_PATH)

# bui = GRFFile('OpenGFX_BigGUI-2.0.0/ogfx-biggui.grf')
# ogfx_extra.load()
# for s in ogfx_extra.g.generators:
#     if isinstance(s, grf.ReplaceOldSprites):
#         print('OLD', s.sets)
#     if isinstance(s, grf.ReplaceNewSprites):
#         print('NEW', s.set_type, s.count, s.offset)

def replace_old_sprites(first_id, amount):
    g.add(grf.ReplaceOldSprites([(first_id, amount)]))
    res = []
    for i in range(amount):
        sprites = ogfx.get_sprites(first_id + i)
        # sprites = [RecolourSprite(s, GREEN) for s in sprites]
        res.append(grf.AlternativeSprites(*sprites))
    g.add(*res)
    return res


def replace_new_sprites(set_type, count, offset, first_id):
    g.add(grf.ReplaceNewSprites(set_type, count, offset=offset))
    res = []
    for i in range(count):
        sprites = ogfx_extra.get_sprites(first_id + i)
        # sprites = [RecolourSprite(s, GREEN) for s in sprites]
        res.append(grf.AlternativeSprites(*sprites))
    g.add(*res)
    return res


replace_new_sprites(11, 4, None, 2488)
# replace_new_sprites(20, 36, None, 2488+112+15+55)
# replace_new_sprites(21, 137, 38, 2488+112+15+55+38+36+6)
# replace_new_sprites(21, 137, 38, 2750)
ofs, sprite = 38, 2750
for x in [16] + [1] * 39 + [17, 4, 23, 1, 8, 1, 6, 9, 13]:
    replace_new_sprites(21, x, ofs, sprite)
    sprite += x + 1
    ofs += x

# Mouse cursors
replace_old_sprites(0, 2)

# arrows and check marks
replace_old_sprites(130, 1)
replace_old_sprites(140, 1)
replace_old_sprites(142, 1)
replace_old_sprites(143, 1)
replace_old_sprites(145, 1)


replace_old_sprites(679, 73)  # Base GUI
replace_old_sprites(1251, 12)  # GUI rails
replace_old_sprites(1263, 12)  # Rail cursors
replace_old_sprites(1291, 1)  # Signal icon
replace_old_sprites(1292, 2)  # Signal cursors
replace_old_sprites(1294, 2)  # Depot icons
replace_old_sprites(1296, 2)  # Depot cursors
replace_old_sprites(1298, 2)  # Rail station icon
replace_old_sprites(1300, 1)  # Rail station cursor
replace_old_sprites(1309, 2)  # Road icons
replace_old_sprites(1311, 2)  # Road cursors
replace_old_sprites(2429, 4)  # Tunnel icons
replace_old_sprites(2433, 4)  # Tunnel cursors
replace_old_sprites(2593, 1)  # Bridge cursor
replace_old_sprites(2594, 7)  # Bridge icons
replace_old_sprites(3090, 2)  # Start-stop icons
replace_old_sprites(4077, 1)  # Town icon
replace_old_sprites(4080, 2)  # Town and industry cursors
replace_old_sprites(4082, 6)  # Object toolbar icons
replace_old_sprites(4791, 1)  # Land own icon
replace_old_sprites(4792, 1)  # Land own cursor

grf.main(g, 'cm_ogfx1_gui.grf')


