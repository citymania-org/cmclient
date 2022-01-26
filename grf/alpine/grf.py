import math

from PIL import Image, ImageDraw
from nml.spriteencoder import SpriteEncoder
import spectra
import struct
import numpy as np

from parser import Node, Expr, Value, Var, Temp, Perm, Call, parse_code, OP_INIT


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

OBJECT = 0x0f


def hex_str(s):
    if isinstance(s, (bytes, memoryview)):
        return ':'.join('{:02x}'.format(b) for b in s)
    return ':'.join('{:02x}'.format(ord(c)) for c in s)


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


def fix_palette(img):
    assert (img.mode == 'P')  # TODO
    pal = tuple(img.getpalette())
    if pal == PALETTE: return img
    print(f'Custom palette in file {img.filename}, converting...')
    # for i in range(256):
    #     if tuple(pal[i * 3: i*3 + 3]) != PALETTE[i * 3: i*3 + 3]:
    #         print(i, pal[i * 3: i*3 + 3], PALETTE[i * 3: i*3 + 3])
    remap = PaletteRemap()
    for i in ALL_COLORS:
        remap.remap[i] = find_best_color(to_spectra(pal[3 * i], pal[3 * i + 1], pal[3 * i + 2]), in_range=ALL_COLORS)
    return remap.remap_image(img)


def open_image(filename, *args, **kw):
    return fix_palette(Image.open(filename, *args, **kw))

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
        self._image = fix_palette(img)
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


class ReplaceSprites(BaseSprite):  # action A
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


class ReplaceNewSprites(BaseSprite):  # action 5
    def __init__(self, set_type, num):  # TODO offset
        assert isinstance(set_type, int)
        assert isinstance(num, int)
        self.set_type = set_type
        self.num = num

    def get_data(self):
        return bytes((0x5,)) + struct.pack('<BBH', self.set_type, 0xff, self.num)

    def get_data_size(self):
        return 5

ACTION0_OBJECT_PROPS = (
    (0x08, 'label', 'L'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Class label, see below
    (0x09, 'class_name_id', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Text ID for class
    (0x0A, 'name_id', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Text ID for this object
    (0x0B, 'climate', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Climate availability
    (0x0C, 'size', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Byte representing size, see below
    (0x0D, 'build_cost_factor', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Object build cost factor (sets object removal cost factor as well)
    (0x0E, 'intro_date', 'D'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Introduction date, see below
    (0x0F, 'eol_date', 'D'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   End of life date, see below
    (0x10, 'flags', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Object flags, see below
    (0x11, 'anim_info', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Animation information
    (0x12, 'anim_speed', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Animation speed
    (0x13, 'anim_trigger', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Animation triggers
    (0x14, 'removal_cost_factor', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Object removal cost factor (set after object build cost factor)
    (0x15, 'cb_flags', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Callback flags, see below
    (0x16, 'building_height', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Height of the building
    (0x17, 'num_views', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Number of object views
    (0x18, 'num_objects', 'B'),  # Supported by OpenTTD 1.4 (r25879)1.4 Not supported by TTDPatch  Measure for number of objects placed upon map creation
)

ACTION0_OBJECT_PROP_DICT = {name: (id, size) for id, name, size in ACTION0_OBJECT_PROPS}


class LazyBaseSprite(BaseSprite):
    def __init__(self):
        super().__init__()
        self._data = None

    def _encode(self):
        raise NotImplemented

    def get_data(self):
        if self._data is None:
            self._data = self._encode()
        return self._data

    def get_data_size(self):
        return len(self.get_data())

class Action0(LazyBaseSprite):  # action 0
    def __init__(self, feature, first_id, count, props):
        super().__init__()
        self.feature = feature
        self.first_id = first_id
        self.count = count
        if feature != OBJECT:
            raise NotImplemented
        self.props = props
        assert all(x in ACTION0_OBJECT_PROP_DICT for x in props)

    def _encode_value(self, value, fmt):
        if fmt == 'B': return struct.pack('<B', value)
        if fmt == 'W': return struct.pack('<H', value)
        if fmt == 'D': return struct.pack('<I', value)
        if fmt == 'L':
            if isinstance(value, int): return struct.pack('<I', value)
            assert isinstance(value, bytes)
            assert len(value) == 4, len(value)
            return value
        if fmt == 'B*': return struct.pack('<BH', 255, value)
        if fmt == 'n*B':
            assert isinstance(value, bytes)
            assert len(value) < 256, len(value)
            return struct.pack('<B', len(value)) + value

    def _encode(self):
        res = struct.pack('<BBBBBH',
            0, self.feature, len(self.props), self.count, 255, self.first_id)
        for prop, value in self.props.items():
            code, fmt = ACTION0_OBJECT_PROP_DICT[prop]
            res += bytes((code,)) + self._encode_value(value, fmt)
        return res


class Object(Action0):
    class Flags:
        NONE               =       0  # Just nothing.
        ONLY_IN_SCENEDIT   = 1 <<  0  # Object can only be constructed in the scenario editor.
        CANNOT_REMOVE      = 1 <<  1  # Object can not be removed.
        AUTOREMOVE         = 1 <<  2  # Object get automatically removed (like "owned land").
        BUILT_ON_WATER     = 1 <<  3  # Object can be built on water (not required).
        CLEAR_INCOME       = 1 <<  4  # When object is cleared a positive income is generated instead of a cost.
        HAS_NO_FOUNDATION  = 1 <<  5  # Do not display foundations when on a slope.
        ANIMATION          = 1 <<  6  # Object has animated tiles.
        ONLY_IN_GAME       = 1 <<  7  # Object can only be built in game.
        CC2_COLOUR         = 1 <<  8  # Object wants 2CC colour mapping.
        NOT_ON_LAND        = 1 <<  9  # Object can not be on land, implicitly sets #OBJECT_FLAG_BUILT_ON_WATER.
        DRAW_WATER         = 1 << 10  # Object wants to be drawn on water.
        ALLOW_UNDER_BRIDGE = 1 << 11  # Object can built under a bridge.
        ANIM_RANDOM_BITS   = 1 << 12  # Object wants random bits in "next animation frame" callback.
        SCALE_BY_WATER     = 1 << 13  # Object count is roughly scaled by water amount at edges.


    def __init__(self, id, **props):
        if 'size' in props:
            props['size'] = (props['size'][1] << 4) | props['size'][0]
        super().__init__(OBJECT, id, 1, props)


class Action1(LazyBaseSprite):
    def __init__(self, feature, set_count, sprite_count):
        super().__init__()
        self.feature = feature
        self.set_count = set_count
        self.sprite_count = sprite_count

    def _encode(self):
        return struct.pack('<BBBBH', 0x01,
                           self.feature, self.set_count, 0xFF, self.sprite_count)


class SpriteSet(Action1):
    def __init__(self, feature, sprite_count):
        super().__init__(feature, 1, sprite_count)


class BasicSpriteLayout(LazyBaseSprite):
    def __init__(self, feature, set_id, xofs=0, yofs=0, extent=(0, 0, 0)):
        super().__init__()
        assert feature in (0x07, 0x09, OBJECT, 0x11), feature
        self.feature = feature
        self.set_id = set_id
        self.xofs = xofs
        self.yofs = yofs
        self.extent = extent

    def _encode_sprite(self, id=0, mode=0, recolor=0, draw_in_transparent=False, use_last=False):
        return id | (mode << 14) | (recolor << 16) | (draw_in_transparent << 30) | (use_last << 31)

    def _encode(self):
        return struct.pack('<BBBBIIbbBBB', 0x02, self.feature, self.set_id, 0,
                           # self._encode_sprite(), self._encode_sprite(),
                           self._encode_sprite(use_last=True), 0,
                           self.xofs, self.yofs,

                           *self.extent)


class AdvancedSpriteLayout(LazyBaseSprite):
    def __init__(self, feature, set_id, ground, sprites=()):
        super().__init__()
        assert feature in (0x07, 0x09, OBJECT, 0x11), feature
        assert len(sprites) < 64, len(sprites)
        self.feature = feature
        self.set_id = set_id
        self.ground = ground
        self.sprites = sprites

    def _encode_sprite(self, sprite, aux=False):
        res = struct.pack('<HHH', sprite['sprite'], sprite['pal'], sprite['flags'])

        if aux:
            delta = sprite.get('delta', (0, 0, 0))
            is_parent = bool(sprite.get('parent'))
            if not is_parent:
                delta = (delta[0], delta[1], 0x80)
            res += struct.pack('<BBB', *delta)
            if is_parent:
                res += struct.pack('<BBB', *sprite.get('size'))

        for k in ('dodraw', 'add', 'palette', 'sprite_var10', 'palette_var10'):
            if k in sprite:
                val = sprite[k]
                if k == 'add':
                    assert isinstance(val, Temp)
                    val = val.register
                res += bytes((val, ))
        # TODO deltas
        return res

    def _encode(self):
        res = struct.pack('<BBBB', 0x02, self.feature, self.set_id, len(self.sprites) + 0x40)
        res += self._encode_sprite(self.ground)
        for s in self.sprites:
            res += self._encode_sprite(s, True)

        return res


class Set:
    def __init__(self, set_id):
        self.value = set_id
        self.is_callback = bool(set_id & 0x8000)
        self.set_id = set_id & 0x7fff

    def __str__(self):
        if self.is_callback:
            return f'CB({self.set_id})'
        return f'Set({self.set_id})'

    __repr__ = __str__


class CB(Set):
    def __init__(self, value):
        super().__init__(value | 0x8000)


class Range:
    def __init__(self, low, high, set):
        self.set = set
        self.low = low
        self.high = high

    def __str__(self):
        if self.low == self.high:
            return f'{self.low} -> {self.set}'
        return f'{self.low}..{self.high} -> {self.set}'

    __repr__ = __str__


class VarAction2(LazyBaseSprite):
    def __init__(self, feature, set_id, use_related, ranges, default, code):
        super().__init__()
        self.feature = feature
        assert feature == OBJECT, feature
        self.set_id = set_id
        self.use_related = use_related
        self.ranges = ranges
        self.default = default
        self.code = parse_code(code)

    def __str__(self):
        return str(self.set_id)

    def _get_set_value(self, set_obj):
        if isinstance(set_obj, Set):
            return set_obj.value
        assert isinstance(set_obj, int)
        return set_obj | 0x8000

    def _encode(self):
        res = bytes((0x02, self.feature, self.set_id, 0x8a if self.use_related else 0x89))
        code = self.code[0].compile(register=0x80)[1]
        for c in self.code[1:]:
            code += bytes((OP_INIT,))
            code += c.compile(register=0x80)[1]
        # print('CODE', hex_str(code))
        res += code[:-5]
        res += bytes((code[-5] & ~0x20,))  # mark the end of a chain
        res += code[-4:]
        res += bytes((len(self.ranges),))
        # print('RES', hex_str(res))
        ranges = self.ranges
        if isinstance(ranges, dict):
            ranges = self.ranges.items()
        for r in ranges:
            if isinstance(r, Range):
                set_obj = r.set
                low = r.low
                high = r.high
            else:
                set_obj = r[1]
                low = r[0]
                high = r[0]
            # TODO split (or validate) negative-positive ranges
            res += struct.pack('<Hii', self._get_set_value(set_obj), low, high)
        res += struct.pack('<H', self._get_set_value(self.default))
        return res


class Action3(LazyBaseSprite):
    def __init__(self, feature, ids, maps, default):
        super().__init__()
        self.feature = feature
        self.ids = ids
        self.maps = maps
        self.default = default

    def _encode(self):
        idcount = len(self.ids)
        mcount = len(self.maps)
        res = struct.pack(
            '<BBB' + 'B' * idcount + 'B' + 'BH' * mcount + 'H',
            0x03, self.feature, idcount,
            *self.ids, mcount, *sum(self.maps, []),
            self.default)
        return res


class Map(Action3):
    def __init__(self, object, maps, default):
        super().__init__(object.feature, [object.first_id], maps, default)


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