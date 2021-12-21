import spectra

import grf


gen = grf.NewGRF(
    b'CMAL',
    'CityMania Alpine Landscape',
    'Modified OpenGFX sprites for alpine climate.',
)


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


def tmpl_level_ground(func, x, y, **kw):
    func(x, y, 64, 31, xofs=-31, yofs=0, **kw)


def tmpl_short_slope(func, x, y, **kw):
    func(x, y, 64, 23, xofs=-31, yofs=0, **kw)


def tmpl_long_slope(func, x, y, **kw):
    func(x, y, 64, 39, xofs=-31, yofs=-8, **kw)


def tmpl_infrastructure_road(func, **kw):
    tmpl_level_ground(func,  82, 40, **kw)
    tmpl_level_ground(func, 162, 40, **kw)
    tmpl_level_ground(func, 242, 40, **kw)
    tmpl_level_ground(func, 322, 40, **kw)
    tmpl_level_ground(func, 402, 40, **kw)
    tmpl_level_ground(func, 482, 40, **kw)
    tmpl_level_ground(func, 562, 40, **kw)
    tmpl_level_ground(func, 642, 40, **kw)
    tmpl_level_ground(func, 722, 40, **kw)
    tmpl_level_ground(func,   2, 88, **kw)
    tmpl_level_ground(func,  82, 88, **kw)
    tmpl_long_slope  (func,  82, 152, **kw)
    tmpl_short_slope (func, 162, 152, **kw)
    tmpl_short_slope (func, 242, 152, **kw)
    tmpl_long_slope  (func, 322, 152, **kw)
    tmpl_level_ground(func, 402, 152, **kw)
    tmpl_level_ground(func, 482, 152, **kw)
    tmpl_level_ground(func, 562, 152, **kw)
    tmpl_level_ground(func, 642, 152, **kw)


def replace_ground_sprites(sprite_id, file, x, y, **kw):
    png = grf.ImageFile(file)
    gen.add_sprite(grf.ReplaceSprites([(sprite_id, 19)]))
    sprite = lambda *args, **kw: gen.add_sprite(grf.FileSprite(png, *args, **kw))
    tmpl_ground_sprites(sprite, x, y, **kw)


def replace_shore_sprites(sprite_id, file, x, y, **kw):
    png = grf.ImageFile(file)
    gen.add_sprite(grf.ReplaceSprites([(sprite_id, 8)]))
    sprite = lambda *args, **kw: gen.add_sprite(grf.FileSprite(png, *args, **kw))
    sprite(320+x,   y, 64, 31, xofs=-31, yofs= 0, **kw)
    sprite( 80+x,   y, 64, 31, xofs=-31, yofs= 0, **kw)
    sprite(160+x,   y, 64, 23, xofs=-31, yofs= 0, **kw)
    sprite(638+x,   y, 64, 39, xofs=-31, yofs=-8, **kw)
    sprite(478+x,   y, 64, 23, xofs=-31, yofs= 0, **kw)
    sprite(958+x,   y, 64, 39, xofs=-31, yofs=-8, **kw)
    sprite(240+x,   y, 64, 23, xofs=-31, yofs= 0, **kw)
    sprite(718+x,   y, 64, 39, xofs=-31, yofs=-8, **kw)


def replace_additional_rough_sprites(sprite_id, file, x, y, **kw):
    png = grf.ImageFile(file)
    gen.add_sprite(grf.ReplaceSprites([(sprite_id, 4)]))
    sprite = lambda *args, **kw: gen.add_sprite(grf.FileSprite(png, *args, **kw))
    sprite(    x, y, 64, 31, xofs=-31, yofs=0, **kw)
    sprite( 80+x, y, 64, 31, xofs=-31, yofs=0, **kw)
    sprite(160+x, y, 64, 31, xofs=-31, yofs=0, **kw)
    sprite(240+x, y, 64, 31, xofs=-31, yofs=0, **kw)

# Normal land
replace_ground_sprites(3981, 'gfx/grass_grid_temperate.gimp.png', 1, 1)

# bulldozed (bare) land and regeneration stages:
replace_ground_sprites(3924, 'gfx/bare03_grid.gimp.png', 1, 1)
replace_ground_sprites(3943, 'gfx/bare13_grid_temperate.gimp.png', 1, 1)
replace_ground_sprites(3962, 'gfx/bare23_grid_temperate.gimp.png', 1, 1)

# rough terrain
replace_ground_sprites(4000, 'gfx/rough_grid_temperate.gimp.png', 1, 1)
replace_additional_rough_sprites(4019, 'gfx/rough_grid_temperate.gimp.png', 1511, 1)

# rocky terrain
replace_ground_sprites(4023, 'gfx/rocks_grid_temperate.gimp.png', 1, 1)

# road sprites
png = grf.ImageFile('gfx/infrastructure/road_grid_temperate.gimp.png')
gen.add_sprite(grf.ReplaceSprites([(1332, 19)]))
tmpl_infrastructure_road(lambda *args, **kw: gen.add_sprite(grf.FileSprite(png, *args, **kw)))

# different snow densities:
# replace_ground_sprites(4493, 'gfx/snow14_grid_alpine.gimp.png', 1, 1)
# replace_ground_sprites(4512, 'gfx/snow24_grid_alpine.gimp.png', 1, 1)
# replace_ground_sprites(4531, 'gfx/snow34_grid_alpine.gimp.png', 1, 1)
replace_ground_sprites(4493, 'gfx/snow_transition_1.png', 1, 1)
replace_ground_sprites(4512, 'gfx/snow_transition_2.png', 1, 1)
replace_ground_sprites(4531, 'gfx/snow_transition_3.png', 1, 1)
replace_ground_sprites(4550, 'gfx/snow_grid.gimp.png', 1, 1)

# shore sprites (replacing 16 seems to do these as well)
# replace_shore_sprites(4062, 'gfx/water/seashore_grid_temperate.gimp.png', 1, 1)

def replace_coastal_sprites(file, x, y, **kw):
    png = grf.ImageFile(file)
    gen.add_sprite(grf.ReplaceNewSprites(0x0d, 16))
    sprite = lambda *args, **kw: gen.add_sprite(grf.FileSprite(png, *args, **kw))
    sprite(1276+x,   y, 64, 15, xofs=-31, yofs=  0, **kw)
    sprite(  80+x,   y, 64, 31, xofs=-31, yofs=  0, **kw)
    sprite( 160+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)
    sprite( 240+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)
    sprite( 320+x,   y, 64, 31, xofs=-31, yofs=  0, **kw)
    sprite(1356+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)
    sprite( 478+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)
    sprite( 558+x,   y, 64, 23, xofs=-31, yofs=  0, **kw)
    sprite( 638+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)
    sprite( 718+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)
    sprite(1196+x,   y, 64, 47, xofs=-31, yofs=-16, **kw)
    sprite( 878+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)
    sprite( 958+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)
    sprite(1038+x,   y, 64, 39, xofs=-31, yofs= -8, **kw)
    sprite(1118+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)
    sprite(1436+x,   y, 64, 31, xofs=-31, yofs= -8, **kw)


replace_coastal_sprites('gfx/water/seashore_grid_temperate.gimp.png', 1, 1)


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
    gen.add_sprite(grf.ReplaceSprites([(sprite_id, 7)]))
    png = grf.ImageFile('gfx/trees/' + file)
    sprite = lambda *args, **kw: gen.add_sprite(grf.FileSprite(png, *args, **kw))
    tmpl_tree_narrow(sprite)


# Tile slope to sprite offset
get_tile_slope_offset = grf.VarAction2(
    feature=grf.OBJECT,
    use_related=False,
    set_id=0,
    ranges={0: 0, 1: 1, 2: 2, 4: 4, 8: 8, 9: 9, 3: 3, 6: 6, 12: 12, 5: 5, 10: 10, 11: 11, 7: 7, 14: 14, 13: 13, 27: 17, 23: 16, 30: 18, 29: 15},
    default=0,
    code='tile_slope'
)
gen.add_sprite(get_tile_slope_offset)

# Ground sprite
get_ground_sprite = grf.VarAction2(
    feature=grf.OBJECT,
    use_related=False,
    set_id=1,
    ranges={-2: 4550 , -1: 4550 - 19, 0: 4550 - 19 * 2, 1: 4550 - 19 * 3},
    default=3981,
    code='max(snowline_height - tile_height, -2)'
)
gen.add_sprite(get_ground_sprite)

# png = grf.ImageFile("gfx/meadow_grid_temperate.png")
# gen.add_sprite(grf.SpriteSet(grf.OBJECT, 19))
# tmpl_ground_sprites(lambda *args, **kw: gen.add_sprite(grf.FileSprite(png, *args, **kw)), 1, 1)
# gen.add_sprite(grf.AdvancedSpriteLayout(
#     grf.OBJECT, 255,
#     ground={
#         'sprite': 0,
#         'pal': 32768,
#         'flags': 2,
#         'add': grf.Temp(0),
#     }
# ))

# gen.add_sprite(grf.Object(0,
#     label=b'FLMA',
#     size=0x11,
#     climate=0xf,
#     eol_date=0,
#     flags=grf.Object.Flags.HAS_NO_FOUNDATION | grf.Object.Flags.ALLOW_UNDER_BRIDGE,
# ))

# gen.add_sprite(grf.VarAction2(
#     feature=grf.OBJECT,
#     use_related=False,
#     set_id=255,
#     ranges={0: grf.Set(255)},
#     default=grf.Set(255),
#     code='TEMP[0] = call(0)'
# ))

# gen.add_sprite(grf.Action3(grf.OBJECT, [0], [[255, 255]], 255))

# CREEKS

gen.add_sprite(grf.Action1(grf.OBJECT, 81, 19))
png = grf.ImageFile("gfx/rivers.png")
for i in range(81):
    tmpl_ground_sprites(lambda *args, **kw: gen.add_sprite(grf.FileSprite(png, *args, **kw)), 1, i * 64 + 1)

for i in range(81):
    gen.add_sprite(grf.AdvancedSpriteLayout(
        grf.OBJECT, 255,
        ground={
            'sprite': 0,
            'pal': 0,
            'flags': 2,
            'add': grf.Temp(1),
        },
        sprites=[{
            'sprite': i,
            'pal': (1 << 15),
            'flags': 2,
            'add': grf.Temp(0),
        }]
    ))

    gen.add_sprite(grf.VarAction2(
        feature=grf.OBJECT,
        use_related=False,
        set_id=255,
        ranges={0: grf.Set(255)},
        default=grf.Set(255),
        code=f'''
            TEMP[0] = call({get_tile_slope_offset})
            TEMP[1] = call({get_ground_sprite}) + TEMP[0]
        '''
    ))
    gen.add_sprite(creek_obj := grf.Object(i,
        label=b'CREE',
        size=(1, 1),
        climate=0xf,
        eol_date=0,
        flags=grf.Object.Flags.HAS_NO_FOUNDATION | grf.Object.Flags.ALLOW_UNDER_BRIDGE | grf.Object.Flags.CANNOT_REMOVE,
    ))


    gen.add_sprite(grf.Map(creek_obj, [[255, 255]], 255))

gen.write('alpine.grf')
