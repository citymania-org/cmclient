import sys
import struct
from nml import lz77


def hex_str(s):
    if isinstance(s, (bytes, memoryview)):
        return ':'.join('{:02x}'.format(b) for b in s)
    return ':'.join('{:02x}'.format(ord(c)) for c in s)


def read_extended_byte(data, offset):
    res = data[offset]
    if res != 0xff:
        return res, offset + 1
    return data[offset + 1] | (data[offset + 2] << 8), offset + 3


def read_dword(data, offset):
    return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24), offset + 4


FEATURES = {
    0: 'Train',
    1: 'RV',
    2: 'Ship',
    3: 'Aircraft',
    4: 'Station',
    5: 'Canal',
    6: 'Bridge',
    7: 'House',
    8: 'Setting',
    9: 'IndTiles',
    0xa: 'Industry',
    0xb: 'Cargo',
    0xc: 'Sound',
    0xd: 'Airport',
    0xe: '?Signals?',
    0xf: 'Object',
    0x10: 'Railtype',
    0x11: 'AirportTiles',
    0x12: 'Roadtype',
    0x13: 'Tramtype',
}

ACTION0_TRAIN_PROPS = {
    0x05:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    Track type (see below)  should be same as front
    0x08:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    AI special flag: set to 1 if engine is 'optimized' for passenger service (AI won't use it for other cargo), 0 otherwise     no
    0x09:  'W',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    Speed in mph*1.6 (see below)    no
    0x0B:  'W',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    Power (0 for wagons)    should be zero
    0x0D:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    Running cost factor (0 for wagons)  should be zero
    0x0E:  'D',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    Running cost base, see below    should be zero
    0x12:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    Sprite ID (FD for new graphics)     yes
    0x13:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    Dual-headed flag; 1 if dual-headed engine, 0 otherwise  should be zero also for front
    0x14:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    Cargo capacity  yes
    0x15:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    Cargo type, see CargoTypes
    0x16:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    Weight in tons  should be zero
    0x17:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0    Cost factor     should be zero
    0x18:  'B',  # Supported by OpenTTD <0.7<0.7 Supported by TTDPatch 2.02.0[1]   Engine rank for the AI (AI selects the highest-rank engine of those it can buy)     no
    0x19:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0 GRFv≥1     Engine traction type (see below)    no
    0x1A:  'B*', # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0 GRFv≥1     Not a property, but an action: sort the purchase list.  no
    0x1B:  'W',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.52.5 GRFv≥6     Power added by each wagon connected to this engine, see below   should be zero
    0x1C:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.52.5 GRFv≥6     Refit cost, using 50% of the purchase price cost base   yes
    0x1D:  'D',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.52.5 GRFv≥6     Bit mask of cargo types available for refitting, see column 2 (bit value) in CargoTypes     yes
    0x1E:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.52.5 GRFv≥6     Callback flags bit mask, see below  yes
    0x1F:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.5 (alpha 19)2.5     Coefficient of tractive effort  should be zero
    0x20:  'B',  # Supported by OpenTTD 1.11.1 Supported by TTDPatch 2.5 (alpha 27)2.5     Coefficient of air drag     should be zero
    0x21:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.02.0 GRFv≥2     Make vehicle shorter by this amount, see below  yes
    0x22:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.52.5 GRFv≥6     Set visual effect type (steam/smoke/sparks) as well as position, see below  yes
    0x23:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.52.5 GRFv≥6     Set how much weight is added by making wagons powered (i.e. weight of engine), see below    should be zero
    0x24:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.5 (alpha 44)2.5     High byte of vehicle weight, weight will be prop.24*256+prop.16     should be zero
    0x25:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.5 (alpha 44)2.5     User-defined bit mask to set when checking veh. var. 42     yes
    0x26:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.5 (alpha 44)2.5     Retire vehicle early, this many years before the end of phase 2 (see Action0General)    no
    0x27:  'B',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.5 (alpha 58)2.5     Miscellaneous flags     partly
    0x28:  'W',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.5 (alpha 58)2.5     Refittable cargo classes    yes
    0x29:  'W',  # Supported by OpenTTD 0.60.6 Supported by TTDPatch 2.5 (alpha 58)2.5     Non-refittable cargo classes    yes
    0x2A:  'D',  # Supported by OpenTTD 0.6 (r7191)0.6 Supported by TTDPatch 2.5 (r1210)2.5    Long format introduction date   no
    0x2B:  'W',  # Supported by OpenTTD 1.2 (r22713)1.2 Not supported by TTDPatch  Custom cargo ageing period  yes
    0x2C:  'n*B', # Supported by OpenTTD 1.2 (r23291)1.2 Not supported by TTDPatch  List of always refittable cargo types   yes
    0x2D:  'n*B',  # Supported by OpenTTD 1.2 (r23291)1.2 Not supported by TTDPatch  List of never refittable cargo types    yes
    0x2E:  'W',  # Supported by OpenTTD 12 (g2183fd4dab)12 Not supported by TTDPatch   Maximum curve speed modifier    yes
}

ACTION0_OBJECT_PROPS = {
    0x08: ('label', 'L'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Class label, see below
    0x09: ('class_name_id', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Text ID for class
    0x0A: ('name_id', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Text ID for this object
    0x0B: ('climate', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Climate availability
    0x0C: ('size', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Byte representing size, see below
    0x0D: ('build_cost_factor', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Object build cost factor (sets object removal cost factor as well)
    0x0E: ('intro_date', 'D'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Introduction date, see below
    0x0F: ('eol_date', 'D'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   End of life date, see below
    0x10: ('flags', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Object flags, see below
    0x11: ('anim_info', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Animation information
    0x12: ('anim_speed', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Animation speed
    0x13: ('anim_trigger', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Animation triggers
    0x14: ('removal_cost_factor', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Object removal cost factor (set after object build cost factor)
    0x15: ('cb_flags', 'W'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Callback flags, see below
    0x16: ('building_height', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Height of the building
    0x17: ('num_views', 'B'),  # Supported by OpenTTD 1.1 (r20670)1.1 Supported by TTDPatch 2.6 (r2340)2.6   Number of object views
    0x18: ('num_objects', 'B'),  # Supported by OpenTTD 1.4 (r25879)1.4 Not supported by TTDPatch  Measure for number of objects placed upon map creation
}

ACTION0_PROPS = {
    0: ACTION0_TRAIN_PROPS,
    0xf: ACTION0_OBJECT_PROPS,
}


def str_feature(feature):
    return f'{FEATURES[feature]}<{feature:02x}>'


def str_sprite(sprite):
    sprite_id = sprite & 0x1fff
    draw = {0: 'N', 1 : 'T', 2: 'R'}[(sprite >> 14) & 3]
    color_translation = (sprite >> 16) & 0x3fff
    normal_in_transparent = bool(sprite & (1 << 30))
    sprite_type = sprite >> 31
    ntstr = ['', ' NF'][normal_in_transparent]
    return f'[{sprite_id} {draw}{ntstr} {color_translation}-{sprite_type}]'


def read_property(data, ofs, fmt):
    if fmt == 'B':
        return data[ofs], ofs + 1

    if fmt == 'W':
        return data[ofs] | (data[ofs + 1] << 8), ofs + 2

    if fmt == 'L':
        return data[ofs: ofs + 4], ofs + 4

    if fmt == 'D':
        return read_dword(data, ofs)

    if fmt == 'B*':
        return read_extended_byte(data, ofs)

    if fmt == 'n*B':
        n = data[ofs]
        return data[ofs + 1: ofs + 1 + n], ofs + 1 + n

    assert False, fmt


def decode_action0(data):
    num = data[0]
    feature = data[0]
    num_props = data[1]
    num_info = data[2]
    first_id, ofs = read_extended_byte(data, 3)
    props = {}
    for _ in range(num_props):
        prop = data[ofs]
        propdict =  ACTION0_PROPS[feature]
        name, fmt = propdict[prop]
        value, ofs = read_property(data, ofs + 1, fmt)
        key = f'{name}<{prop:02x}>'
        assert key not in props, key
        props[key] = value

    print(f'    <0>FEATURE feature:{str_feature(feature)} num_info:{num_info} first_id:{first_id} props:{props}')


def decode_action1(data):
    num = data[0]
    feature = data[0]
    num_sets = data[1]
    num_ent, _ = read_extended_byte(data, 2)
    print(f'    <1>SPRITESET feature:{str_feature(feature)} num_sets:{num_sets} num_ent:{num_ent}')


SPRITE_GROUP_OP = [
    'ADD',   # a + b
    'SUB',   # a - b
    'SMIN',  # (signed) min(a, b)
    'SMAX',  # (signed) max(a, b)
    'UMIN',  # (unsigned) min(a, b)
    'UMAX',  # (unsigned) max(a, b)
    'SDIV',  # (signed) a / b
    'SMOD',  # (signed) a % b
    'UDIV',  # (unsigned) a / b
    'UMOD',  # (unsigned) a & b
    'MUL',   # a * b
    'AND',   # a & b
    'OR',    # a | b
    'XOR',   # a ^ b
    'STO',   # store a into temporary storage, indexed by b. return a
    'RST',   # return b
    'STOP',  # store a into persistent storage, indexed by b, return a
    'ROR',   # rotate a b positions to the right
    'SCMP',  # (signed) comparison (a < b -> 0, a == b = 1, a > b = 2)
    'UCMP',  # (unsigned) comparison (a < b -> 0, a == b = 1, a > b = 2)
    'SHL',   # a << b
    'SHR',   # (unsigned) a >> b
    'SAR',   # (signed) a >> b
]

class DataReader:
    def __init__(self, data, offset=0):
        self.data = data
        self.offset = offset

    def get_byte(self):
        self.offset += 1
        return self.data[self.offset - 1]

    def get_extended_byte(self):
        res, self.offset = read_extended_byte(self.data, self.offset)
        return res

    def get_word(self):
        return self.get_byte() | (self.get_byte() << 8)

    def get_var(self, n):
        size = 1 << n
        res = struct.unpack_from({0: '<B', 1: '<H', 2: '<I'}[n], self.data, offset=self.offset)[0]
        self.offset += size
        return res


TLF_NOTHING           = 0x00
TLF_DODRAW            = 0x01  # Only draw sprite if value of register TileLayoutRegisters::dodraw is non-zero.
TLF_SPRITE            = 0x02  # Add signed offset to sprite from register TileLayoutRegisters::sprite.
TLF_PALETTE           = 0x04  # Add signed offset to palette from register TileLayoutRegisters::palette.
TLF_CUSTOM_PALETTE    = 0x08  # Palette is from Action 1 (moved to SPRITE_MODIFIER_CUSTOM_SPRITE in palette during loading).
TLF_BB_XY_OFFSET      = 0x10  # Add signed offset to bounding box X and Y positions from register TileLayoutRegisters::delta.parent[0..1].
TLF_BB_Z_OFFSET       = 0x20  # Add signed offset to bounding box Z positions from register TileLayoutRegisters::delta.parent[2].
TLF_CHILD_X_OFFSET    = 0x10  # Add signed offset to child sprite X positions from register TileLayoutRegisters::delta.child[0].
TLF_CHILD_Y_OFFSET    = 0x20  # Add signed offset to child sprite Y positions from register TileLayoutRegisters::delta.child[1].
TLF_SPRITE_VAR10      = 0x40  # Resolve sprite with a specific value in variable 10.
TLF_PALETTE_VAR10     = 0x80  # Resolve palette with a specific value in variable 10.
TLF_KNOWN_FLAGS       = 0xFF  # Known flags. Any unknown set flag will disable the GRF.

# /** Flags which are still required after loading the GRF. */
TLF_DRAWING_FLAGS     = ~TLF_CUSTOM_PALETTE

# /** Flags which do not work for the (first) ground sprite. */
TLF_NON_GROUND_FLAGS  = TLF_BB_XY_OFFSET | TLF_BB_Z_OFFSET | TLF_CHILD_X_OFFSET | TLF_CHILD_Y_OFFSET

# /** Flags which refer to using multiple action-1-2-3 chains. */
TLF_VAR10_FLAGS       = TLF_SPRITE_VAR10 | TLF_PALETTE_VAR10

# /** Flags which require resolving the action-1-2-3 chain for the sprite, even if it is no action-1 sprite. */
TLF_SPRITE_REG_FLAGS  = TLF_DODRAW | TLF_SPRITE | TLF_BB_XY_OFFSET | TLF_BB_Z_OFFSET | TLF_CHILD_X_OFFSET | TLF_CHILD_Y_OFFSET

# /** Flags which require resolving the action-1-2-3 chain for the palette, even if it is no action-1 palette. */
TLF_PALETTE_REG_FLAGS = TLF_PALETTE


def read_sprite_layout_registers(d, flags, is_parent):
    regs = {'flags': flags & TLF_DRAWING_FLAGS}
    if flags & TLF_DODRAW:  regs['dodraw']  = d.get_byte();
    if flags & TLF_SPRITE:  regs['sprite']  = d.get_byte();
    if flags & TLF_PALETTE: regs['palette'] = d.get_byte();

    if is_parent:
        delta = [d.get_byte(), d.get_byte(), 0] if flags & TLF_BB_XY_OFFSET else [0, 0, 0]
        if flags & TLF_BB_Z_OFFSET: delta[2] = d.get_byte()
        regs['delta_parent'] = tuple(delta)
    else:
        delta, delta_set = [0, 0], False
        if flags & TLF_CHILD_X_OFFSET: delta[0], delta_set = d.get_byte(), True
        if flags & TLF_CHILD_Y_OFFSET: delta[1], delta_set = d.get_byte(), True
        if delta_set: regs['delta_child'] = tuple(delta)

    if flags & TLF_SPRITE_VAR10: regs['sprite_var10'] = d.get_byte()
    if flags & TLF_PALETTE_VAR10: regs['palette_var10'] = d.get_byte()
    return regs


def read_sprite_layout(d, num, no_z_position):
    has_z_position = not no_z_position
    has_flags = bool((num >> 6) & 1)
    num &= 0x3f

    def read_sprite():
        sprite = d.get_word()
        pal = d.get_word()
        flags = d.get_word() if has_flags else TLF_NOTHING
        return {'sprite': sprite, 'pal': pal, 'flags': flags}

    ground = read_sprite()
    ground_regs = read_sprite_layout_registers(d, ground['flags'], False)
    sprites = []
    for _ in range(num):
        seq = {}
        seq['sprite'] = read_sprite()
        delta = seq['delta'] = (d.get_byte(), d.get_byte(), d.get_byte() if has_z_position else 0)
        is_parent = (delta[2] != 0x80)
        if is_parent:
            seq['size'] = (d.get_byte(), d.get_byte(), d.get_byte())
        seq['regs'] = read_sprite_layout_registers(d, seq['sprite']['flags'], is_parent)
        sprites.append(seq)

    return {
        'ground': {
            'sprite': ground,
            'regs': ground_regs,
        },
        'sprites': sprites
    }

def decode_action2(data):
    feature = data[0]
    set_id = data[1]
    num_ent1 = data[2]
    d = DataReader(data, 3)

    print(f'    <2>SPRITEGROUP feature:{str_feature(feature)} set_id:{set_id} ', end='')

    if feature in (0x07, 0x09, 0x0f, 0x11):
        if num_ent1 == 0:
            ground_sprite, building_sprite, xofs, yofs, xext, yext, zext = struct.unpack_from('<IIBBBBB', data, offset=3)
            ground_sprite = str_sprite(ground_sprite)
            building_sprite = str_sprite(building_sprite)
            print(f'ground_sprite:{ground_sprite} building_sprite:{building_sprite} '
                  f'xofs:{xofs} yofs:{yofs} extent:({xext}, {yext}, {zext})')
            return
        if num_ent1 < 0x3f:
            raise NotImplemented

        if num_ent1 in (0x81, 0x82, 0x85, 0x86, 0x89, 0x8a):
            group_size = (num_ent1 >> 2) & 3
            first = True
            ofs = 3
            adjusts = []
            while True:
                res = {}
                res['op'] = 0 if first else d.get_byte()
                var = res['var'] = d.get_byte()
                if var == 0x7e:
                    res['subroutine'] = d.get_byte()
                else:
                    res['parameter'] = d.get_byte() if 0x60 <= var < 0x80 else 0
                varadj = d.get_byte()
                res['shift_num'] = varadj & 0x1f
                has_more = bool(varadj & 0x20)
                res['type'] = varadj >> 6
                res['and_mask'] = d.get_var(group_size)
                if res['type'] != 0:
                    res['add_val'] = d.get_var(group_size)
                    res['divmod_val'] = d.get_var(group_size)
                adjusts.append(res)

                if not has_more:
                    break

            n_ranges = d.get_byte()
            ranges = []
            for _ in range(n_ranges):
                group = d.get_word()
                low = d.get_var(group_size)
                high = d.get_var(group_size)
                ranges.append((group, low, high))

            default_group = d.get_word()

            print(f'default_group: {default_group} adjusts:{adjusts} ranges:{ranges} ')

            return

        layout = read_sprite_layout(d, max(num_ent1, 1), num_ent1 == 0)
        print(f'layout:{layout}')
        return
        # num_loaded = num_ent1
        # num_loading = get_byte()
        # [get_word() for i in range(num_loaded)]
        # [get_word() for i in range(num_loading)]
        # assert False, num_ent1
        # # assert num_ent1 < 0x3f + 0x40, num_ent1
        # return

    num_ent2 = data[3]
    ent1 = struct.unpack_from('<' + 'H' * num_ent1, data, offset=4)
    ent2 = struct.unpack_from('<' + 'H' * num_ent2, data, offset=4 + 2 * num_ent1)
    print(f'ent1:{ent1} ent2:{ent2}')


def decode_action4(data):
    fmt = '<BBB' + ('H' if data[1] & 0xf0 else 'B')
    feature, lang, num, offset = struct.unpack_from(fmt, data)
    strings = [s.decode('utf-8') for s in data[struct.calcsize(fmt):].split(b'\0')[:-1]]
    print(f'    <4>STRINGS feature:{str_feature(feature)} lang:{lang} num:{num} offset:{offset} strings:{strings}')


def decode_action5(data):
    t = data[0]
    offset = 0
    num, dataofs = read_extended_byte(data, 1)
    if t & 0xf0:
        offset, _ = read_extended_byte(data, dataofs)
        t &= ~0xf0
    print(f'    <5>REPLACENEW type:{t} num:{num}, offset:{offset}')


def decode_action6(data):
    d = DataReader(data, 0)
    params = []
    while True:
        param_num = d.get_byte()
        if param_num == 0xFF:
            break
        param_size = d.get_byte()
        offset = d.get_extended_byte()
        params.append({'num': param_num, 'size': param_size, 'offset': offset})
    print(f'    <6>EDITPARAM params:{params}')


def decode_actionA(data):
    num = data[0]
    sets = [struct.unpack_from('<BH', data, offset=3*i + 1) for i in range(num)]
    print(f'    <A>REPLACEBASE sets:<{num}>{sets}')


OPERATIONS = {
    0x00: '{target} = {source1}', # Supported by OpenTTD Supported by TTDPatch  Assignment  target = source1
    0x01: '{target} = {source1} + {source2}', # Supported by OpenTTD Supported by TTDPatch  Addition    target = source1 + source2
    0x02: '{target} = {source1} - {source2}', # Supported by OpenTTD Supported by TTDPatch  Subtraction     target = source1 - source2
    0x03: '{target} = {source1} * {source2} (Unsigned)', # Supported by OpenTTD Supported by TTDPatch  Unsigned multiplication     target = source1 * source2, with both sources being considered to be unsigned
    0x04: '{target} = {source1} * {source2} (Signed)', # Supported by OpenTTD Supported by TTDPatch  Signed multiplication   target = source1 * source2, with both sources considered signed
    0x05: '{target} = {source1} <</>> {source2} (Unsigned)', # Supported by OpenTTD Supported by TTDPatch  Unsigned bit shift  target = source1 << source2 if source2>0, or target = source1 >> abs(source2) if source2 < 0. source1 is considered to be unsigned
    0x06: '{target} = {source1} <</>> {source2} (Signed)', # Supported by OpenTTD Supported by TTDPatch  Signed bit shift    same as 05, but source1 is considered signed)
    0x07: '{target} = {source1} & {source2}', # Supported by OpenTTD Supported by TTDPatch 2.5 (alpha 48)2.5    Bitwise AND     target = source1 AND source2
    0x08: '{target} = {source1} | {source2}', # Suported by OpenTTD Supported by TTDPatch 2.5 (alpha 48)2.5    Bitwise OR  target = source1 OR source2
    0x09: '{target} = {source1} / {source2} (Unsigned)', # Supported by OpenTTD Supported by TTDPatch 2.5 (alpha 59)2.5    Unsigned division   target = source1 / source2
    0x0A: '{target} = {source1} / {source2} (Signed)', # Supported by OpenTTD Supported by TTDPatch 2.5 (alpha 59)2.5    Signed division     target = source1 / source2
    0x0B: '{target} = {source1} % {source2} (Unsigned)', # Supported by OpenTTD Supported by TTDPatch 2.5 (alpha 59)2.5    Unsigned modulo     target = source1 % source2
    0x0C: '{target} = {source1} % {source2} (Signed)', # Supported by OpenTTD Supported by TTDPatch 2.5 (alpha 59)2.5    Signed modulo   target = source1 % source2
}


def decode_actionD(data):
    target = data[0]
    operation = data[1]
    source1 = data[2]
    source2 = data[3]
    if source1 == 0xff or source2 == 0xff:
        value, _ = read_dword(data, 4)
    fmt = OPERATIONS[operation]
    sf = lambda x: f'[{x:02x}]' if x != 0xff else str(value)
    target_str = f'[{target:02x}]'
    op_str = fmt.format(target=target_str, source1=sf(source1), source2=sf(source2))
    print(f'    <A>OP {op_str}')


def decode_action14(data):
    res = {}
    ofs = 0

    def decode_chunk(res):
        nonlocal ofs
        chunk_type = data[ofs]
        ofs += 1
        if chunk_type == 0: return False
        chunk_id = data[ofs: ofs + 4]
        ofs += 4
        if chunk_type == b'C'[0]:
            res[chunk_id] = {}
            while decode_chunk(res[chunk_id]):
                pass
        elif chunk_type == b'B'[0]:
            l = data[ofs] | (data[ofs + 1] << 8)
            res[chunk_id] = data[ofs + 2: ofs + 2 + l]
            ofs += 2 + l
        # elif chunk_type == b'T'[0]:
        else:
            assert False, chunk_type
        return True

    while decode_chunk(res):
        pass

    print(f'    <14>INFO {res}')


ACTIONS = {
    0x00: decode_action0,
    0x01: decode_action1,
    0x02: decode_action2,
    0x04: decode_action4,
    0x05: decode_action5,
    0x06: decode_action6,
    0x0a: decode_actionA,
    0x0d: decode_actionD,
    0x14: decode_action14,
}

def read_pseudo_sprite(f):
    l = struct.unpack('<I', f.read(4))[0]
    if l == 0:
        print('End of pseudo sprites')
        return False
    grf_type = f.read(1)[0]
    grf_type_str = hex(grf_type)[2:]
    data = f.read(l)
    print(f'Sprite({l}, {grf_type_str}): ', hex_str(data[:100]))
    if grf_type == 0xff:
        decoder = ACTIONS.get(data[0])
        if decoder:
            decoder(data[1:])
    return True

def decode_sprite(f, num):
    data = b''
    while num > 0:
        code = f.read(1)[0]
        if code >= 128: code -= 256
        # print(f'Code {code} num {num}')
        if code >= 0:
            size = 0x80 if code == 0 else code
            num -= size
            if num < 0: raise RuntimeError('Corrupt sprite')
            data += f.read(size)
        else:
            data_offset = ((code & 7) << 8) | f.read(1)[0]
            #if (dest - data_offset < dest_orig.get()) return WarnCorruptSprite(file, file_pos, __LINE__);
            size = -(code >> 3)
            num -= size
            if num < 0: raise RuntimeError('Corrupt sprite')
            data += data[-data_offset:size - data_offset]
    if num != 0: raise RuntimeError('Corrupt sprite')
    return data

def read_real_sprite(f):
    sprite_id = struct.unpack('<I', f.read(4))[0]
    if sprite_id == 0:
        print(f'End of real sprites')
        return False
    print(f'Real sprite({sprite_id}): ', end='')
    num, t = struct.unpack('<IB', f.read(5))
    start_pos = f.tell()
    print(f'({num}, {t:02x}): ', end='')
    if t == 0xff:
        print('non-real (skip)')
        f.seek(start_pos + num - 1, 0)
        return True
    zoom, height, width, x_offs, y_offs = struct.unpack('<BHHhh', f.read(9))
    bpp = 1  # TODO
    decomp_size = struct.unpack('<I', f.read(4))[0] if t & 0x08 else width * height * bpp
    print(f'{width}x{height} zoom={zoom} x_offs={x_offs} y_offs={y_offs} bpp={bpp} decomp_size={decomp_size}')
    # data = decode_sprite(f, decomp_size)
    # print('Data: ', hex_str(data[:40]))
    f.seek(start_pos + num - 1, 0)
    return True

with open(sys.argv[1], 'rb') as f:
    print('Header:', hex_str(f.read(10)))
    data_offest, compression = struct.unpack('<IB', f.read(5))
    header_offset = f.tell() - 1
    print(f'Offset: {data_offest} compresion: {compression}')
    while read_pseudo_sprite(f):
        pass
    real_data_offset = f.tell() - header_offset
    # while read_real_sprite(f):
    #     pass
    if data_offest != real_data_offset:
        print(f'[ERROR] Data offset check failed: {data_offest} {real_data_offset}')

