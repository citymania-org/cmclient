import math

from PIL import Image


TILESHAPES = "sprites/tileshapes.png"
SPITE_MARGIN = 10
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

def pointdist(x, y, p):
    xx, yy = p
    return math.hypot(xx - x, yy - y)

def linedist(x, y, p1, p2):
    x1, y1 = p1
    x2, y2 = p2
    return abs((y2 - y1) * x - (x2 - x1) * y + x2 * y1 - y2 * x1) / math.hypot(y2 - y1, x2 - x1)

def gen_borders(tile, borders, corner):
    D = 4
    w, h = tile.size

    res = tile.copy()
    pixels = res.load()

    mx = w // 2
    my1, my2 = 0, 0
    for i in range(h):
        if pixels[0, i]:
            my1 = i
        if pixels[w - 1, i]:
            my2 = i

    ptop = (32, -1)  # -.5 extra instead of adding .5 to pixels
    pbottom = (32, h)
    pleft = (0, my1)
    pright = (64, my2)
    for y in range(h):
        for x in range(w):
            if pixels[x, y] == 0:
                continue
            d = D + 1
            if borders & 1:
                d = min(d, linedist(x, y, ptop, pleft))

            if borders & 2:
                d = min(d, linedist(x, y, ptop, pright))

            if borders & 4:
                d = min(d, linedist(x, y, pbottom, pright))

            if borders & 8:
                d = min(d, linedist(x, y, pbottom, pleft))

            if corner:
                d = min(d, pointdist(x, y, [None, ptop, pright, pbottom, pleft][corner]))
            # d = D + 1
            # dxa, dxb, dya, dyb = 0, 0, 0, 0
            # if borders & 1 and x < mx and y <= my1:
            #     dxa, dya, d = -D, -D, min(d, x + 1, y + 1)
            # if borders & 2 and x >= mx and y <= my2:
            #     dxb, dya, d = D, -D, min(d, w - x, y + 1)
            # if borders & 4 and x >= mx and y > my2:
            #     dxb, dyb, d = D, D, min(d, w - x, h - y)
            # if borders & 8 and x < mx and y > my1:
            #     dxa, dyb, d = -D, D, min(d, x + 1, h - y)

            # for j in range(max(0, y + dya), min(h, y + dyb + 1)):
            #     for i in range(max(0, x + dxa), min(w, x + dxb + 1)):
            #         if pixels[i, j] == 0:
            #             d = min(d, dist(i, j, x, y))
            # d = min(d, dist(x, y, 31, -1), dist(x, y, 32, -1))
            # d = min(d, dist(x, y, 31, h), dist(x, y, 32, h))
            # d = min(d, dist(x, y, -1, my1), dist(x, y, w, my2))
            if d <= D:
                pixels[x, y] = 0x0f - int(d * 1.4)
            else:
                # pixels[x, y] = int(bool(pixels[x, y])) * 0xD7
                pixels[x, y] = 0

    return res

def process_tile(iimg, oimg, tiledata, x_ofs, y_ofs):
    x, y, w, h, ox, oy = tiledata
    tile = iimg.crop((x, y, x + w, y + h))

    def add_sprite(res):
        nonlocal x_ofs
        oimg.paste(res, (x_ofs, y_ofs))
        print(f"[{x_ofs:4}, {y_ofs:4}, {w:2}, {h:2}, {ox:3}, {oy:3}]")
        x_ofs += w + SPITE_MARGIN

    for borders in range(1, 16):
        add_sprite(gen_borders(tile, borders, 0))

    for corner in range(1, 5):
        add_sprite(gen_borders(tile, 0, corner))

    return h


iimg = Image.open(TILESHAPES)
oimg = iimg.crop((0, 0, 19 * (64 + SPITE_MARGIN) + SPITE_MARGIN, 789))
oimg.load()
oimg.paste(0xf, (0, 0, oimg.width, oimg.height))

x_ofs = SPITE_MARGIN
y_ofs = SPITE_MARGIN
for p in TILEDATA:
    h = process_tile(iimg, oimg, p, x_ofs, y_ofs)
    y_ofs += h + SPITE_MARGIN
oimg.save('sprites/borderhighlight.png')
oimg.show()
