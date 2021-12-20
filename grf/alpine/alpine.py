import grf
import spectra

gen = grf.NewGRF(
    b'CMAL',
    'CityMania Alpine Landscape',
    'Modified OpenGFX spritess for alpine climate.',
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

# different snow densities:
replace_ground_sprites(4493, 'gfx/snow14_grid_alpine.gimp.png', 1, 1)
replace_ground_sprites(4512, 'gfx/snow24_grid_alpine.gimp.png', 1, 1)
replace_ground_sprites(4531, 'gfx/snow34_grid_alpine.gimp.png', 1, 1)
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

# Tile slope to sprite offset
gen.add_sprite(grf.VarAction2(
    feature=grf.OBJECT,
    use_related=False,
    set_id=0,
    ranges={0: 0, 1: 1, 2: 2, 4: 4, 8: 8, 9: 9, 3: 3, 6: 6, 12: 12, 5: 5, 10: 10, 11: 11, 7: 7, 14: 14, 13: 13, 27: 17, 23: 16, 30: 18, 29: 15},
    default=0,
    code='tile_slope'
))

# Ground sprite
gen.add_sprite(grf.VarAction2(
    feature=grf.OBJECT,
    use_related=False,
    set_id=1,
    ranges={-2: 4550 , -1: 4550 - 19, 0: 4550 - 19 * 2, 1: 4550 - 19 * 3},
    default=3981,
    code='max(snowline_height - tile_height, -2)'
))


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
            TEMP[0] = call(0)
            TEMP[1] = call(1) + TEMP[0]
        '''
    ))

    gen.add_sprite(creek_obj := grf.Object(i,
        label=b'CREE',
        size=(1, 1),
        climate=0xf,
        eol_date=0,
        flags=grf.Object.Flags.HAS_NO_FOUNDATION | grf.Object.Flags.ALLOW_UNDER_BRIDGE,
    ))


    gen.add_sprite(grf.Map(creek_obj, [[255, 255]], 255))

gen.write('alpine.grf')
