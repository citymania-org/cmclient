import math

from PIL import Image, ImageDraw
from nml.spriteencoder import SpriteEncoder
import spectra
import struct
import numpy as np

to_spectra = lambda r, g, b: spectra.rgb(float(r) / 255., float(g) / 255., float(b) / 255.)
# working with DOS palette only
PALETTE = (0, 0, 255, 16, 16, 16, 32, 32, 32, 48, 48, 48, 64, 64, 64, 80, 80, 80, 100, 100, 100, 116, 116, 116, 132, 132, 132, 148, 148, 148, 168, 168, 168, 184, 184, 184, 200, 200, 200, 216, 216, 216, 232, 232, 232, 252, 252, 252, 52, 60, 72, 68, 76, 92, 88, 96, 112, 108, 116, 132, 132, 140, 152, 156, 160, 172, 176, 184, 196, 204, 208, 220, 48, 44, 4, 64, 60, 12, 80, 76, 20, 96, 92, 28, 120, 120, 64, 148, 148, 100, 176, 176, 132, 204, 204, 168, 72, 44, 4, 88, 60, 20, 104, 80, 44, 124, 104, 72, 152, 132, 92, 184, 160, 120, 212, 188, 148, 244, 220, 176, 64, 0, 4, 88, 4, 16, 112, 16, 32, 136, 32, 52, 160, 56, 76, 188, 84, 108, 204, 104, 124, 220, 132, 144, 236, 156, 164, 252, 188, 192, 252, 208, 0, 252, 232, 60, 252, 252, 128, 76, 40, 0, 96, 60, 8, 116, 88, 28, 136, 116, 56, 156, 136, 80, 176, 156, 108, 196, 180, 136, 68, 24, 0, 96, 44, 4, 128, 68, 8, 156, 96, 16, 184, 120, 24, 212, 156, 32, 232, 184, 16, 252, 212, 0, 252, 248, 128, 252, 252, 192, 32, 4, 0, 64, 20, 8, 84, 28, 16, 108, 44, 28, 128, 56, 40, 148, 72, 56, 168, 92, 76, 184, 108, 88, 196, 128, 108, 212, 148, 128, 8, 52, 0, 16, 64, 0, 32, 80, 4, 48, 96, 4, 64, 112, 12, 84, 132, 20, 104, 148, 28, 128, 168, 44, 28, 52, 24, 44, 68, 32, 60, 88, 48, 80, 104, 60, 104, 124, 76, 128, 148, 92, 152, 176, 108, 180, 204, 124, 16, 52, 24, 32, 72, 44, 56, 96, 72, 76, 116, 88, 96, 136, 108, 120, 164, 136, 152, 192, 168, 184, 220, 200, 32, 24, 0, 56, 28, 0, 72, 40, 4, 88, 52, 12, 104, 64, 24, 124, 84, 44, 140, 108, 64, 160, 128, 88, 76, 40, 16, 96, 52, 24, 116, 68, 40, 136, 84, 56, 164, 96, 64, 184, 112, 80, 204, 128, 96, 212, 148, 112, 224, 168, 128, 236, 188, 148, 80, 28, 4, 100, 40, 20, 120, 56, 40, 140, 76, 64, 160, 100, 96, 184, 136, 136, 36, 40, 68, 48, 52, 84, 64, 64, 100, 80, 80, 116, 100, 100, 136, 132, 132, 164, 172, 172, 192, 212, 212, 224, 40, 20, 112, 64, 44, 144, 88, 64, 172, 104, 76, 196, 120, 88, 224, 140, 104, 252, 160, 136, 252, 188, 168, 252, 0, 24, 108, 0, 36, 132, 0, 52, 160, 0, 72, 184, 0, 96, 212, 24, 120, 220, 56, 144, 232, 88, 168, 240, 128, 196, 252, 188, 224, 252, 16, 64, 96, 24, 80, 108, 40, 96, 120, 52, 112, 132, 80, 140, 160, 116, 172, 192, 156, 204, 220, 204, 240, 252, 172, 52, 52, 212, 52, 52, 252, 52, 52, 252, 100, 88, 252, 144, 124, 252, 184, 160, 252, 216, 200, 252, 244, 236, 72, 20, 112, 92, 44, 140, 112, 68, 168, 140, 100, 196, 168, 136, 224, 200, 176, 248, 208, 184, 255, 232, 208, 252, 60, 0, 0, 92, 0, 0, 128, 0, 0, 160, 0, 0, 196, 0, 0, 224, 0, 0, 252, 0, 0, 252, 80, 0, 252, 108, 0, 252, 136, 0, 252, 164, 0, 252, 192, 0, 252, 220, 0, 252, 252, 0, 204, 136, 8, 228, 144, 4, 252, 156, 0, 252, 176, 48, 252, 196, 100, 252, 216, 152, 8, 24, 88, 12, 36, 104, 20, 52, 124, 28, 68, 140, 40, 92, 164, 56, 120, 188, 72, 152, 216, 100, 172, 224, 92, 156, 52, 108, 176, 64, 124, 200, 76, 144, 224, 92, 224, 244, 252, 200, 236, 248, 180, 220, 236, 132, 188, 216, 88, 152, 172, 244, 0, 244, 245, 0, 245, 246, 0, 246, 247, 0, 247, 248, 0, 248, 249, 0, 249, 250, 0, 250, 251, 0, 251, 252, 0, 252, 253, 0, 253, 254, 0, 254, 255, 0, 255, 76, 24, 8, 108, 44, 24, 144, 72, 52, 176, 108, 84, 210, 146, 126, 252, 60, 0, 252, 84, 0, 252, 104, 0, 252, 124, 0, 252, 148, 0, 252, 172, 0, 252, 196, 0, 64, 0, 0, 255, 0, 0, 48, 48, 0, 64, 64, 0, 80, 80, 0, 255, 255, 0, 32, 68, 112, 36, 72, 116, 40, 76, 120, 44, 80, 124, 48, 84, 128, 72, 100, 144, 100, 132, 168, 216, 244, 252, 96, 128, 164, 68, 96, 140, 255, 255, 255)
SAFE_COLORS = set(range(1, 0xD7))
ALL_COLORS = set(range(256))
SPECTRA_PALETTE = {i:to_spectra(PALETTE[i * 3], PALETTE[i * 3 + 1], PALETTE[i * 3 + 2]) for i in range(256)}
WATER_COLORS = set(range(0xF5, 0xFF))

# ZOOM_OUT_4X, ZOOM_NORMAL, ZOOM_OUT_2X, ZOOM_OUT_8X, ZOOM_OUT_16X, ZOOM_OUT_32X = range(6)
ZOOM_4X, ZOOM_NORMAL, ZOOM_2X, ZOOM_8X, ZOOM_16X, ZOOM_32X = range(6)
BPP_8, BPP_32 = range(2)

def color_distance(c1, c2):
    rmean = (c1.rgb[0] + c2.rgb[0]) / 2.
    r = c1.rgb[0] - c2.rgb[0]
    g = c1.rgb[1] - c2.rgb[1]
    b = c1.rgb[2] - c2.rgb[2]
    return math.sqrt(
        ((2 + rmean) * r * r) +
        4 * g * g +
        (3 - rmean) * b * b)


def find_best_color(x, in_range=SAFE_COLORS):
    mj, md = 0, 1e100
    for j in in_range:
        c = SPECTRA_PALETTE[j]
        d = color_distance(x, c)
        if d < md:
            mj, md = j, d
    return mj


# def map_rgb_image(self, im):
#     assert im.mode == 'RGB', im.mode
#     data = np.array(im)


class BaseSprite:
    def get_data(self):
        raise NotImplemented

    def get_data_size(self):
        raise NotImplemented


class PaletteRemap(BaseSprite):
    def __init__(self, ranges=None):
        self.remap = np.arange(256, dtype=np.uint8)
        if ranges:
            self.set_ranges(ranges)

    def get_data(self):
        return b'\x00' + self.remap.tobytes()

    def get_data_size(self):
        return 257

    @classmethod
    def from_function(cls, color_func, remap_water=False):
        res = cls()
        for i in SAFE_COLORS:
            res.remap[i] = find_best_color(color_func(SPECTRA_PALETTE[i]))
        if remap_water:
            for i in WATER_COLORS:
                res.remap[i] = find_best_color(color_func(SPECTRA_PALETTE[i]))
        return res

    def set_ranges(self, ranges):
        for r in ranges:
            f, t, v = r
            self.remap[f: t + 1] = v

    def remap_image(self, im):
        assert im.mode == 'P', im.mode
        data = np.array(im)
        data = self.remap[data]
        res = Image.fromarray(data)
        res.putpalette(PALETTE)
        return res


class RealSprite(BaseSprite):
    def __init__(self, w, h, *, xofs=0, yofs=0, zoom=ZOOM_4X):
        self.sprite_id = None
        self.w = w
        self.h = h
        # self.file = None
        # self.x = None
        # self.y = None
        self.xofs = xofs
        self.yofs = yofs
        self.zoom = zoom

    def get_data_size(self):
        return 4

    def get_data(self):
        return struct.pack('<I', self.sprite_id)

    def get_real_data(self):
        raise NotImplementedError

    def draw(self, img):
        raise NotImplementedError


class ImageFile:
    def __init__(self, filename, bpp=BPP_8):
        assert(bpp == BPP_8)  # TODO
        self.filename = filename
        self.bpp = bpp
        self._image = None

    def get_image(self):
        if self._image:
            return self._image
        img = Image.open(self.filename)
        assert (img.mode == 'P')  # TODO
        pal = tuple(img.getpalette())
        if pal != PALETTE:
            print(f'Custom palette in file {self.filename}, converting...')
            # for i in range(256):
            #     if tuple(pal[i * 3: i*3 + 3]) != PALETTE[i * 3: i*3 + 3]:
            #         print(i, pal[i * 3: i*3 + 3], PALETTE[i * 3: i*3 + 3])
            remap = PaletteRemap()
            for i in ALL_COLORS:
                remap.remap[i] = find_best_color(to_spectra(pal[3 * i], pal[3 * i + 1], pal[3 * i + 2]), in_range=ALL_COLORS)
            self._image = remap.remap_image(img)
        else:
            self._image = img
        return self._image


class FileSprite(RealSprite):
    def __init__(self, file, x, y, w, h, **kw):
        assert(isinstance(file, ImageFile))
        super().__init__(w, h, **kw)
        self.file = file
        self.x = x
        self.y = y

    def get_real_data(self):
        img = self.file.get_image()
        img = img.crop((self.x, self.y, self.x + self.w, self.y + self.h))
        raw_data = img.tobytes()
        se = SpriteEncoder(True, False, None)
        data = se.sprite_compress(raw_data)
        return struct.pack(
            '<IIBBHHhh',
            self.sprite_id,
            len(data) + 10, 0x04,
            self.zoom,
            self.h,
            self.w,
            self.xofs,
            self.yofs,
        ) + data
        # return struct.pack(
        #     '<IIBBHHhhI',
        #     self.sprite_id,
        #     len(data) + 14, 0x0C,
        #     self.zoom,
        #     self.h,
        #     self.w,
        #     self.xofs,
        #     self.yofs,
        #     len(raw_data),
        # ) + data



class SpriteSheet:
    def __init__(self, sprites=None):
        self._sprites = list(sprites) if sprites else []

    def make_image(self, filename, padding=5, columns=10):
        w, h = 0, padding
        lineofs = []
        for i in range(0, len(self._sprites), columns):
            w = max(w, sum(s.w for s in self._sprites[i: i + columns]))
            lineofs.append(h)
            h += padding + max((s.h for s in self._sprites[i: i + columns]), default=0)

        w += (columns + 1) * padding
        im = Image.new('L', (w, h), color=0xff)
        im.putpalette(PALETTE)

        x = 0
        for i, s in enumerate(self._sprites):
            y = lineofs[i // columns]
            if i % columns == 0:
                x = padding
            s.x = x
            s.y = y
            s.file = filename
            s.draw(im)
            x += s.w + padding

        im.save(filename)


    def write_nml(self, file):
        for s in self._sprites:
            file.write(s.get_nml())
            file.write('\n')
        file.write('\n')


class DummySprite(BaseSprite):
    def get_data(self):
        return b'\x00'

    def get_data_size(self):
        return 1


class DescriptionSprite(BaseSprite):  # action 8
    def __init__(self, grfid, name, description):
        assert isinstance(grfid, bytes)
        assert isinstance(name, str)
        assert isinstance(description, str)
        self.grfid = grfid
        self.name = name
        self.description = description
        self._data = b'\x08\x08' + self.grfid + self.name.encode('utf-8') + b'\x00' + self.description.encode('utf-8') + b'\x00'

    def get_data(self):
        return self._data

    def get_data_size(self):
        return len(self._data)


class InformationSprite(BaseSprite):  # action 14
    def __init__(self, palette):  # TODO everything else
        # self.palette = {'D': b'\x00', 'W': b'\x01', 'A': b'\x02'}[palette]
        self.palette = palette.encode('utf-8')
        self._data = b'\x14CINFOBPALS\x01\x00' + self.palette + b'\x00\x00'

    def get_data(self):
        return self._data

    def get_data_size(self):
        return len(self._data)


class ReplaceSprites:  # action A
    def __init__(self, sets):
        assert isinstance(sets, (list, tuple))
        assert len(sets) <= 0xff
        for first, num in sets:
            assert isinstance(first, int)
            assert isinstance(num, int)

        self.sets = sets

    def get_data(self):
        return bytes((0xa, len(self.sets))) + b''.join(struct.pack('<BH', num, first) for first, num in self.sets)

    def get_data_size(self):
        return 2 + 3 * len(self.sets)


class ReplaceNewSprites:  # action 5
    def __init__(self, set_type, num):  # TODO offset
        assert isinstance(set_type, int)
        assert isinstance(num, int)
        self.set_type = set_type
        self.num = num

    def get_data(self):
        return bytes((0x5,)) + struct.pack('<BBH', self.set_type, 0xff, self.num)

    def get_data_size(self):
        return 5


class NewGRF:
    def __init__(self, grfid, name, description):
        self.sprites = []
        self.pseudo_sprites = []
        self.pseudo_sprites.append(InformationSprite('D'))
        self.pseudo_sprites.append(DescriptionSprite(grfid, name, description))
        self._next_sprite_id = 1

    def add_sprite(self, *sprites):
        assert(len(sprites) > 0)
        if isinstance(sprites[0], RealSprite):
            assert(all(isinstance(s, RealSprite) for s in sprites))
            assert(len(set(s.zoom for s in sprites)) == len(sprites))
            for s in sprites:
                s.sprite_id = self._next_sprite_id
            self._next_sprite_id += 1

            for s in sprites:
                self.sprites.append(s)

            self.pseudo_sprites.append(sprites[0])
        else:
            assert(len(sprites) == 1)
            self.pseudo_sprites.append(sprites[0])

    def _write_pseudo_sprite(self, f, data, grf_type=0xff):
        f.write(struct.pack('<IB', len(data), grf_type))
        f.write(data)

    def write(self, filename):
        data_offset = 14
        for s in self.pseudo_sprites:
            data_offset += s.get_data_size() + 5

        with open(filename, 'wb') as f:
            f.write(b'\x00\x00GRF\x82\x0d\x0a\x1a\x0a')  # file header
            f.write(struct.pack('<I', data_offset))
            f.write(b'\x00')  # compression(1)
            # f.write(b'\x04\x00\x00\x00')  # num(4)
            # f.write(b'\xFF')  # grf_type(1)
            # f.write(b'\xb0\x01\x00\x00')  # num + 0xff -> recoloursprites() (257 each)
            self._write_pseudo_sprite(f, b'\x02\x00\x00\x00')

            for s in self.pseudo_sprites:
                self._write_pseudo_sprite(f, s.get_data(), grf_type=0xfd if isinstance(s, RealSprite) else 0xff)
            f.write(b'\x00\x00\x00\x00')
            for s in self.sprites:
                f.write(s.get_real_data())

            f.write(b'\x00\x00\x00\x00')