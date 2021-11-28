print("""\
grf {
    grfid: "CMAL";
    name: string(STR_GRF_NAME);
    desc: string(STR_GRF_DESCRIPTION);
    version: 1;
    min_compatible_version: 1;
}

template tmpl_groundsprites_flags(x, y, flags) {
                                    // N E S W STEEP
    [   0+x,   y, 64, 31, -31,  0, flags ] //
    [  80+x,   y, 64, 31, -31,  0, flags ] //       W
    [ 160+x,   y, 64, 23, -31,  0, flags ] //     S
    [ 240+x,   y, 64, 23, -31,  0, flags ] //     S W

    [ 320+x,   y, 64, 31, -31,  0, flags ] //   E
    [ 398+x,   y, 64, 31, -31,  0, flags ] //   E   W
    [ 478+x,   y, 64, 23, -31,  0, flags ] //   E S
    [ 558+x,   y, 64, 23, -31,  0, flags ] //   E S W

    [ 638+x,   y, 64, 39, -31, -8, flags ] // N
    [ 718+x,   y, 64, 39, -31, -8, flags ] // N     W
    [ 798+x,   y, 64, 31, -31, -8, flags ] // N   S
    [ 878+x,   y, 64, 31, -31, -8, flags ] // N   S W

    [ 958+x,   y, 64, 39, -31, -8, flags ] // N E
    [1038+x,   y, 64, 39, -31, -8, flags ] // N E   W
    [1118+x,   y, 64, 31, -31, -8, flags ] // N E S
    [1196+x,   y, 64, 47, -31,-16, flags ] // N E   W STEEP

    [1276+x,   y, 64, 15, -31,  0, flags ] //   E S W STEEP
    [1356+x,   y, 64, 31, -31, -8, flags ] // N   S W STEEP
    [1436+x,   y, 64, 31, -31, -8, flags ] // N E S   STEEP
}

template tmpl_groundsprites(x, y) {
    tmpl_groundsprites_flags(x, y, 0)
}

template tmpl_groundsprites_anim(x, y) {
    tmpl_groundsprites_flags(x, y, ANIM)
}

template tmpl_level_ground(x, y) {
    [ x, y, 64, 31, -31,  0 ]
}

template tmpl_rough(x, y) {
    tmpl_level_ground(    x, y)
    tmpl_level_ground( 80+x, y)
    tmpl_level_ground(160+x, y)
    tmpl_level_ground(240+x, y)
}

template tmpl_additional_rough(x, y) {
    tmpl_rough(1510+x, y)
}

template tmpl_16shore_tiles(x, y) {
    [1276+x,   y, 64, 15, -31,  0 ]
    [  80+x,   y, 64, 31, -31,  0 ]
    [ 160+x,   y, 64, 23, -31,  0 ]
    [ 240+x,   y, 64, 23, -31,  0 ]

    [ 320+x,   y, 64, 31, -31,  0 ]
    [1356+x,   y, 64, 31, -31, -8 ]
    [ 478+x,   y, 64, 23, -31,  0 ]
    [ 558+x,   y, 64, 23, -31,  0 ]

    [ 638+x,   y, 64, 39, -31, -8 ]
    [ 718+x,   y, 64, 39, -31, -8 ]
    [1196+x,   y, 64, 47, -31,-16 ]
    [ 878+x,   y, 64, 31, -31, -8 ]

    [ 958+x,   y, 64, 39, -31, -8 ]
    [1038+x,   y, 64, 39, -31, -8 ]
    [1118+x,   y, 64, 31, -31, -8 ]
    [1436+x,   y, 64, 31, -31, -8 ]
}

template tmpl_8shore_tiles(x, y) {
    [ 320+x,   y, 64, 31, -31,  0 ]
    [  80+x,   y, 64, 31, -31,  0 ]
    [ 160+x,   y, 64, 23, -31,  0 ]
    [ 638+x,   y, 64, 39, -31, -8 ]

    [ 478+x,   y, 64, 23, -31,  0 ]
    [ 958+x,   y, 64, 39, -31, -8 ]
    [ 240+x,   y, 64, 23, -31,  0 ]
    [ 718+x,   y, 64, 39, -31, -8 ]
}

template tmpl_tree_wide() {
    [  0, 0, 45, 80, -24, -73]
    [ 50, 0, 45, 80, -24, -73]
    [100, 0, 45, 80, -24, -73]
    [150, 0, 45, 80, -24, -73]
    [200, 0, 45, 80, -24, -73]
    [250, 0, 45, 80, -24, -73]
    [300, 0, 45, 80, -24, -73]
}

template tmpl_tree_narrow() {
    [  0, 0, 35, 80, -19, -73]
    [ 40, 0, 35, 80, -19, -73]
    [ 80, 0, 35, 80, -19, -73]
    [120, 0, 35, 80, -19, -73]
    [160, 0, 35, 80, -19, -73]
    [200, 0, 35, 80, -19, -73]
    [240, 0, 35, 80, -19, -73]
}

// Normal land:
replace (3981, "gfx/grass_grid_temperate.gimp.png") { tmpl_groundsprites(1, 1) }

// bulldozed (bare) land and regeneration stages:
replace (3924, "gfx/bare03_grid.gimp.png") { tmpl_groundsprites(1, 1) }
replace (3943, "gfx/bare13_grid_temperate.gimp.png") { tmpl_groundsprites(1, 1) }
replace (3962, "gfx/bare23_grid_temperate.gimp.png") { tmpl_groundsprites(1, 1) }

// rough terrain
replace (4000, "gfx/rough_grid_temperate.gimp.png") { tmpl_groundsprites(1, 1) }
replace (4019, "gfx/rough_grid_temperate.gimp.png") { tmpl_additional_rough(1, 1) }

// rocky terrain
replace (4023, "gfx/rocks_grid_temperate.gimp.png") { tmpl_groundsprites(1, 1) }

// different snow densities:
replace (4493, "gfx/snow14_grid_alpine.gimp.png") { tmpl_groundsprites(1,  1) }
replace (4512, "gfx/snow24_grid_alpine.gimp.png") { tmpl_groundsprites(1,  1) }
replace (4531, "gfx/snow34_grid_alpine.gimp.png") { tmpl_groundsprites(1,  1) }
replace (4550, "gfx/snow_grid.gimp.png") { tmpl_groundsprites(1,  1) }

// shore sprites
replace (4062, "gfx/water/seashore_grid_temperate.gimp.png") { tmpl_8shore_tiles(1,    1) }
replacenew (COAST_TILES, "gfx/water/seashore_grid_temperate.gimp.png") { tmpl_16shore_tiles(1,    1) }


// //Arctic trees: the following trees have snowy equivalents:
// base_graphics spr1709(1709, "trees/tree_01_conifer.gimp.png") { tmpl_tree_narrow() }      // 1709 conifer (snowy: 1765)
// base_graphics spr1716(1716, "trees/tree_06_leaf.gimp.png") { tmpl_tree_narrow() }         // 1716 leaf tree (snowy: 1772)
// base_graphics spr1723(1723, "trees/tree_07_leaf.gimp.png") { tmpl_tree_narrow() }         // 1723 leaf tree (snowy: 1779)
// base_graphics spr1730(1730, "trees/tree_08_conifer.gimp.png") { tmpl_tree_narrow() }      // 1730 conifer (snowy: 1786)
// base_graphics spr1737(1737, "trees/tree_09_conifer.gimp.png") { tmpl_tree_narrow() }      // 1737 conifer (snowy: 1793)
// base_graphics spr1744(1744, "trees/tree_04_conifer.gimp.png") { tmpl_tree_narrow() }      // 1744 conifer (snowy: 1800)
// base_graphics spr1751(1751, "trees/tree_05_conifer.gimp.png") { tmpl_tree_narrow() }      // 1751 conifer (snowy: 1807)
// base_graphics spr1758(1758, "trees/tree_10_leaf.gimp.png") { tmpl_tree_narrow() }         // 1758 leaf tree (snowy: 1814)
// // snowy trees
// base_graphics spr1765(1765, "trees/tree_01_snow_conifer.gimp.png") { tmpl_tree_narrow() } // 1765 snowy conifer (equiv of 1709)
// base_graphics spr1772(1772, "trees/tree_06_snow_leaf.gimp.png") { tmpl_tree_narrow() }    // 1772 snowy leaf tree (equiv. of 1716)
// base_graphics spr1779(1779, "trees/tree_07_snow_leaf.gimp.png") { tmpl_tree_narrow() }    // 1779 snowy leaf tree (equiv. of 1723)
// base_graphics spr1786(1786, "trees/tree_08_snow_conifer.gimp.png") { tmpl_tree_narrow() } // 1786 snowy conifer (equiv. of 1730)
// base_graphics spr1793(1793, "trees/tree_09_snow_conifer.gimp.png") { tmpl_tree_narrow() } // 1793 snowy conifer (equiv. of 1737)
// base_graphics spr1800(1800, "trees/tree_04_snow_conifer.gimp.png") { tmpl_tree_narrow() } // 1800 snowy conifer (equiv. of 1744)
// base_graphics spr1807(1807, "trees/tree_05_snow_conifer.gimp.png") { tmpl_tree_narrow() } // 1807 snowy conifer (equiv. of 1751)
// base_graphics spr1814(1814, "trees/tree_10_snow_leaf.gimp.png") { tmpl_tree_narrow() }    // 1814 snowy leaf tree (equiv. of 1758)

spriteset (meadow_groundsprites, "gfx/meadow_grid_temperate.png") { tmpl_groundsprites(1, 1) }


spriteset (meadow_transitions, "gfx/meadow_transitions.png") {
    tmpl_groundsprites(1, 0 * 64 + 1)
    tmpl_groundsprites(1, 1 * 64 + 1)
    tmpl_groundsprites(1, 2 * 64 + 1)
    tmpl_groundsprites(1, 3 * 64 + 1)
    tmpl_groundsprites(1, 4 * 64 + 1)
    tmpl_groundsprites(1, 5 * 64 + 1)
    tmpl_groundsprites(1, 6 * 64 + 1)
    tmpl_groundsprites(1, 7 * 64 + 1)
    tmpl_groundsprites(1, 8 * 64 + 1)
    tmpl_groundsprites(1, 9 * 64 + 1)
    tmpl_groundsprites(1, 10 * 64 + 1)
    tmpl_groundsprites(1, 11 * 64 + 1)
    tmpl_groundsprites(1, 12 * 64 + 1)
    tmpl_groundsprites(1, 13 * 64 + 1)
    tmpl_groundsprites(1, 14 * 64 + 1)
    tmpl_groundsprites(1, 15 * 64 + 1)
}

spritelayout meadow_groundsprites_default {
    ground {
        sprite: meadow_transitions(
            slope_to_sprite_offset(tile_slope)
            + (nearby_tile_object_type( 0, -1) == meadow && nearby_tile_object_type(-1, -1) == meadow &&nearby_tile_object_type(-1, 0) == meadow ? 0 : 19)
            + (nearby_tile_object_type(-1,  0) == meadow && nearby_tile_object_type(-1,  1) == meadow &&nearby_tile_object_type( 0,  1) == meadow ? 0 : 38)
            + (nearby_tile_object_type( 0,  1) == meadow && nearby_tile_object_type( 1,  1) == meadow &&nearby_tile_object_type( 1,  0) == meadow ? 0 : 76)
            + (nearby_tile_object_type( 1,  0) == meadow && nearby_tile_object_type( 1, -1) == meadow &&nearby_tile_object_type( 0, -1) == meadow ? 0 : 152)
        );
    }
}

spritelayout meadow_groundsprites_purchase {
    ground {
        sprite: meadow_groundsprites;
    }
}


switch (FEAT_OBJECTS, SELF, switch_meadow_groundsprites_default, [
        STORE_TEMP(slope_to_sprite_offset(tile_slope), 0)
        ]) {
    meadow_groundsprites_default;
}


item (FEAT_OBJECTS, meadow) {
    property {
        class: "FLMA";
        classname: string(STR_FLMA);
        name: string(STR_TEST_OBJECT);
        climates_available: ALL_CLIMATES;
        end_of_life_date: 0xFFFFFFFF;
        object_flags:bitmask(OBJ_FLAG_ALLOW_BRIDGE, OBJ_FLAG_ANYTHING_REMOVE, OBJ_FLAG_NO_FOUNDATIONS);
        size: [1,1];
    }
    graphics {
        default: meadow_groundsprites_default;
        purchase: meadow_groundsprites_purchase;
        tile_check: CB_RESULT_LOCATION_ALLOW;
    }
}

spriteset (creek_groundsprites, "gfx/rivers.png") {
    tmpl_groundsprites_anim(1, 0 * 64 + 1)
    tmpl_groundsprites_anim(1, 1 * 64 + 1)
    tmpl_groundsprites_anim(1, 2 * 64 + 1)
    tmpl_groundsprites_anim(1, 3 * 64 + 1)
    tmpl_groundsprites_anim(1, 4 * 64 + 1)
    tmpl_groundsprites_anim(1, 5 * 64 + 1)
    tmpl_groundsprites_anim(1, 6 * 64 + 1)
    tmpl_groundsprites_anim(1, 7 * 64 + 1)
    tmpl_groundsprites_anim(1, 8 * 64 + 1)
    tmpl_groundsprites_anim(1, 9 * 64 + 1)
    tmpl_groundsprites_anim(1, 10 * 64 + 1)
    tmpl_groundsprites_anim(1, 11 * 64 + 1)
    tmpl_groundsprites_anim(1, 12 * 64 + 1)
    tmpl_groundsprites_anim(1, 13 * 64 + 1)
    tmpl_groundsprites_anim(1, 14 * 64 + 1)
    tmpl_groundsprites_anim(1, 15 * 64 + 1)
    tmpl_groundsprites_anim(1, 16 * 64 + 1)
    tmpl_groundsprites_anim(1, 17 * 64 + 1)
    tmpl_groundsprites_anim(1, 18 * 64 + 1)
    tmpl_groundsprites_anim(1, 19 * 64 + 1)
    tmpl_groundsprites_anim(1, 20 * 64 + 1)
    tmpl_groundsprites_anim(1, 21 * 64 + 1)
    tmpl_groundsprites_anim(1, 22 * 64 + 1)
    tmpl_groundsprites_anim(1, 23 * 64 + 1)
    tmpl_groundsprites_anim(1, 24 * 64 + 1)
    tmpl_groundsprites_anim(1, 25 * 64 + 1)
    tmpl_groundsprites_anim(1, 26 * 64 + 1)
    tmpl_groundsprites_anim(1, 27 * 64 + 1)
    tmpl_groundsprites_anim(1, 28 * 64 + 1)
    tmpl_groundsprites_anim(1, 29 * 64 + 1)
    tmpl_groundsprites_anim(1, 30 * 64 + 1)
    tmpl_groundsprites_anim(1, 31 * 64 + 1)
    tmpl_groundsprites_anim(1, 32 * 64 + 1)
    tmpl_groundsprites_anim(1, 33 * 64 + 1)
    tmpl_groundsprites_anim(1, 34 * 64 + 1)
    tmpl_groundsprites_anim(1, 35 * 64 + 1)
    tmpl_groundsprites_anim(1, 36 * 64 + 1)
    tmpl_groundsprites_anim(1, 37 * 64 + 1)
    tmpl_groundsprites_anim(1, 38 * 64 + 1)
    tmpl_groundsprites_anim(1, 39 * 64 + 1)
    tmpl_groundsprites_anim(1, 40 * 64 + 1)
    tmpl_groundsprites_anim(1, 41 * 64 + 1)
    tmpl_groundsprites_anim(1, 42 * 64 + 1)
    tmpl_groundsprites_anim(1, 43 * 64 + 1)
    tmpl_groundsprites_anim(1, 44 * 64 + 1)
    tmpl_groundsprites_anim(1, 45 * 64 + 1)
    tmpl_groundsprites_anim(1, 46 * 64 + 1)
    tmpl_groundsprites_anim(1, 47 * 64 + 1)
    tmpl_groundsprites_anim(1, 48 * 64 + 1)
    tmpl_groundsprites_anim(1, 49 * 64 + 1)
    tmpl_groundsprites_anim(1, 50 * 64 + 1)
    tmpl_groundsprites_anim(1, 51 * 64 + 1)
    tmpl_groundsprites_anim(1, 52 * 64 + 1)
    tmpl_groundsprites_anim(1, 53 * 64 + 1)
    tmpl_groundsprites_anim(1, 54 * 64 + 1)
    tmpl_groundsprites_anim(1, 55 * 64 + 1)
    tmpl_groundsprites_anim(1, 56 * 64 + 1)
    tmpl_groundsprites_anim(1, 57 * 64 + 1)
    tmpl_groundsprites_anim(1, 58 * 64 + 1)
    tmpl_groundsprites_anim(1, 59 * 64 + 1)
    tmpl_groundsprites_anim(1, 60 * 64 + 1)
    tmpl_groundsprites_anim(1, 61 * 64 + 1)
    tmpl_groundsprites_anim(1, 62 * 64 + 1)
    tmpl_groundsprites_anim(1, 63 * 64 + 1)
    tmpl_groundsprites_anim(1, 64 * 64 + 1)
    tmpl_groundsprites_anim(1, 65 * 64 + 1)
    tmpl_groundsprites_anim(1, 66 * 64 + 1)
    tmpl_groundsprites_anim(1, 67 * 64 + 1)
    tmpl_groundsprites_anim(1, 68 * 64 + 1)
    tmpl_groundsprites_anim(1, 69 * 64 + 1)
    tmpl_groundsprites_anim(1, 70 * 64 + 1)
    tmpl_groundsprites_anim(1, 71 * 64 + 1)
    tmpl_groundsprites_anim(1, 72 * 64 + 1)
    tmpl_groundsprites_anim(1, 73 * 64 + 1)
    tmpl_groundsprites_anim(1, 74 * 64 + 1)
    tmpl_groundsprites_anim(1, 75 * 64 + 1)
    tmpl_groundsprites_anim(1, 76 * 64 + 1)
    tmpl_groundsprites_anim(1, 77 * 64 + 1)
    tmpl_groundsprites_anim(1, 78 * 64 + 1)
    tmpl_groundsprites_anim(1, 79 * 64 + 1)
    tmpl_groundsprites_anim(1, 80 * 64 + 1)
}

spritelayout creek_groundsprites_default(n, tile_height) {
    ground {
        sprite:
            (climate == CLIMATE_ARCTIC && tile_height > snowline_height - 2 ?
                GROUNDSPRITE_SNOW + min(tile_height - snowline_height - 2, 0) * 19
                : (terrain_type == TILETYPE_DESERT ? GROUNDSPRITE_DESERT : GROUNDSPRITE_NORMAL))
            + slope_to_sprite_offset(tile_slope);
    }
    childsprite {
        sprite: creek_groundsprites(
            slope_to_sprite_offset(tile_slope) + 19 * n
        );
        always_draw: 1;
    }
}

spritelayout creek_groundsprites_purchase(n) {
    ground { sprite: GROUNDSPRITE_NORMAL; }
    childsprite {
        sprite: creek_groundsprites(19 * n);
        always_draw: 1;
    }
}

""")

print('spriteset (mine_number_sprites, "gfx/mine_numbers.png") {')
for i in range(10):
    print(f'    [{i * 12}, 0, 12, 16, -6, 8]')
print('}')

print(f""" \
spritelayout minefield_sprites(n) {{
    ground {{ sprite: GROUNDSPRITE_NORMAL + slope_to_sprite_offset(tile_slope); }}
    childsprite {{
        sprite: mine_number_sprites(n);
        always_draw: 1;
    }}
}}
""")

print(f""" \
item (FEAT_OBJECTS, minefield) {{
    property {{
        class: "MINE";
        classname: string(STR_MINEFIELD);
        name: string(STR_TEST_OBJECT);
        climates_available: ALL_CLIMATES;
        end_of_life_date: 0xFFFFFFFF;
        object_flags:bitmask(OBJ_FLAG_ANYTHING_REMOVE, OBJ_FLAG_NO_FOUNDATIONS);
        size: [1,1];
        num_views: 2;
    }}
    graphics {{
        default: minefield_sprites(
                (
                    (nearby_tile_object_type(-1, -1) == minefield) +
                    (nearby_tile_object_type(-1, 0) == minefield) +
                    (nearby_tile_object_type(-1, 1) == minefield) +
                    (nearby_tile_object_type(0, -1) == minefield) +
                    (nearby_tile_object_type(0, 1) == minefield) +
                    (nearby_tile_object_type(1, -1) == minefield) +
                    (nearby_tile_object_type(1, 0) == minefield) +
                    (nearby_tile_object_type(1, 1) == minefield)
                ) == 8 ? 9 : (
                    (nearby_tile_object_view(-1, -1)) +
                    (nearby_tile_object_view(-1, 0)) +
                    (nearby_tile_object_view(-1, 1)) +
                    (nearby_tile_object_view(0, -1)) +
                    (nearby_tile_object_view(0, 1)) +
                    (nearby_tile_object_view(0, 0)) +
                    (nearby_tile_object_view(1, -1)) +
                    (nearby_tile_object_view(1, 0)) +
                    (nearby_tile_object_view(1, 1))
                )
            );
        purchase: minefield_sprites(9);
        tile_check: CB_RESULT_LOCATION_ALLOW;
    }}
}}
""")

for i in range(81):
    print(f"""\
item (FEAT_OBJECTS, rivers_{i}) {{
    property {{
        class: "CREE";
        classname: string(STR_CREEK);
        name: string(STR_TEST_OBJECT);
        climates_available: ALL_CLIMATES;
        end_of_life_date: 0xFFFFFFFF;
        object_flags:bitmask(OBJ_FLAG_ALLOW_BRIDGE, OBJ_FLAG_ANYTHING_REMOVE, OBJ_FLAG_NO_FOUNDATIONS);
        size: [1,1];
    }}
    graphics {{
        default: creek_groundsprites_default({i}, nearby_tile_height(0, 0));
        purchase: creek_groundsprites_purchase({i});
        tile_check: CB_RESULT_LOCATION_ALLOW;
    }}
}}
""")
