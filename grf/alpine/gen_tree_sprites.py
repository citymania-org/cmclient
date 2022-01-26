import os

from PIL import Image, ImageDraw
import numpy as np

import grf

# SOURCE_DIR = "/home/pavels/Projects/cmclient/local/ogfx-landscape-1.1.2-source/src/gfx"
SOURCE_DIR = "gfx/trees_ogfx"

class TreeImageFile(grf.ImageFile):
    def get_image(self):
        if self._image is not None:
            return self._image
        im = super().get_image()
        data = np.array(im)
        assert im.size == (275, 80), im.size
        for i in range(7):
            for y in range(80):
                for x in range(30):
                    px = i * 40 + x
                    color = data[y, px]
                    if color == 0: continue
                    sc = grf.SPECTRA_PALETTE[color]
                    sc = sc.darken(abs(15 - x))
                    sc.saturate(10)
                    data[y, px] = 20 # grf.find_best_color(sc)
        return im


def tmpl_tree_narrow(func, **kw):
    func(  0, 0, 35, 80, xofs=-19, yofs=-73, **kw)
    func( 40, 0, 35, 80, xofs=-19, yofs=-73, **kw)
    func( 80, 0, 35, 80, xofs=-19, yofs=-73, **kw)
    func(120, 0, 35, 80, xofs=-19, yofs=-73, **kw)
    func(160, 0, 35, 80, xofs=-19, yofs=-73, **kw)
    func(200, 0, 35, 80, xofs=-19, yofs=-73, **kw)
    func(240, 0, 35, 80, xofs=-19, yofs=-73, **kw)


TREES = [
    (1709, 'tree_01_conifer.gimp.png'),
    (1765, 'tree_01_snow_conifer.gimp.png'),
    (1744, 'tree_04_conifer.gimp.png'),
    (1800, 'tree_04_snow_conifer.gimp.png'),
    (1751, 'tree_05_conifer.gimp.png'),
    (1807, 'tree_05_snow_conifer.gimp.png'),
    (1716, 'tree_06_leaf.gimp.png'),
    (1772, 'tree_06_snow_leaf.gimp.png'),
    (1723, 'tree_07_leaf.gimp.png'),
    (1779, 'tree_07_snow_leaf.gimp.png'),
    (1730, 'tree_08_conifer.gimp.png'),
    (1786, 'tree_08_snow_conifer.gimp.png'),
    (1737, 'tree_09_conifer.gimp.png'),
    (1793, 'tree_09_snow_conifer.gimp.png'),
    (1758, 'tree_10_leaf.gimp.png'),
    (1814, 'tree_10_snow_leaf.gimp.png'),
]


for sprite_id, file in TREES:
    print(file, '... ', end='')
    im = Image.open(os.path.join(SOURCE_DIR, file))
    data = np.array(im)
    assert im.size == (275, 80), im.size
    pal = im.getpalette()
    sp_pal = {i:grf.to_spectra(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]) for i in range(256)}

    for i in range(7):
        minx, maxx = 29, 0
        for y in range(80):
            for x in range(30):
                if data[y, i * 40 + x] != 0:
                    maxx = max(maxx, x)
                    minx = min(minx, x)

        midx = minx + (maxx - minx) * 2 / 3
        mscale = 15. / (maxx - minx)

        for y in range(80):
            for x in range(30):
                px = i * 40 + x
                color = data[y, px]
                if color == 0: continue
                sc = sp_pal[color]
                if sum(sc.rgb) > 0.6:
                    sc = sc.darken(mscale * abs(x - midx))
                data[y, px] = grf.find_best_color(sc)

    im2 = Image.fromarray(data)
    im2.putpalette(grf.PALETTE)
    im2.save('gfx/trees/' + file)
    print('Done')
