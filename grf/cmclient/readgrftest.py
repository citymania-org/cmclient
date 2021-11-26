import sys
import struct
from nml import lz77


def hex_str(s):
    if isinstance(s, (bytes, memoryview)):
        return ':'.join('{:02x}'.format(b) for b in s)
    return ':'.join('{:02x}'.format(ord(c)) for c in s)


def read_pseudo_sprite(f):
    l = struct.unpack('<I', f.read(4))[0]
    if l == 0:
        print('End of pseudo sprites')
        return False
    grf_type = str(f.read(1))[-3:-1]
    data = f.read(l)
    print(f'Sprite({l}, {grf_type}): ', hex_str(data[:100]))
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
    while read_real_sprite(f):
        pass
    if data_offest != real_data_offset:
        print(f'[ERROR] Data offset check failed: {data_offest} {real_data_offset}')

