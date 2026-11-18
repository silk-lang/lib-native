// Cursor images for the keywords a system has no cursor of its own for: every host draws
// these through its custom cursor path. Each is 32 x 32: '#' black, 'o' white, 'x' black
// with a white edge (Windows inverts the screen there), anything else clear.
//
// grab, grabbing, col-resize, row-resize, vertical-text, copy, alias, zoom-in, zoom-out and
// cell are Chromium's Windows cursors (ui/resources/cursors), BSD-3-Clause:
// Copyright 2015 The Chromium Authors. Use of this source code is governed by a BSD-style
// license that can be found at https://chromium.googlesource.com/chromium/src/+/main/LICENSE.
// context-menu, no-drop and move are drawn here in the same style.
#ifndef SILK_WINDOW_CURSORS_H
#define SILK_WINDOW_CURSORS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define SILK_CURSOR_ART 32

typedef struct SilkCursorArt {
    uint32_t cursor; // types.Cursor index
    uint8_t hotX, hotY, first, count;
    const char *rows[SILK_CURSOR_ART];
} SilkCursorArt;

static const SilkCursorArt silk_cursor_art[] = {
    {4, 13, 13, 7, 14, { // grabbing
        "............##",
        ".........###oo###",
        "........#oo#oo#oo#",
        "........#oo#oo#oo##",
        "........#oooooooo#o#",
        ".......##oooooooooo#",
        "......#oo#ooooooooo#",
        "......#oo#ooooooooo#",
        "......#ooooooooooo#",
        "......#ooooooooooo#",
        ".......#ooooooooo#",
        "........#oooooooo#",
        "........#ooooooo#",
        ".........#######",
    }},
    {5, 13, 13, 3, 18, { // grab
        "............##",
        "........##.#oo###",
        ".......#oo##oo#oo#",
        ".......#ooo#oo#oo#",
        ".......#ooo#oo#oo###",
        "........#oo#oo#oo#oo#",
        "........#oo#oo#oo#oo#",
        "........#oo#oo#oo#oo#",
        "....###.#oooooooo#oo#",
        "...#ooo##oooooooooo#",
        "...#ooooo#ooooooooo#",
        "....#oooo#ooooooooo#",
        ".....#oooooooooooo#",
        "......#ooooooooooo#",
        ".......#ooooooooo#",
        "........#oooooooo#",
        "........#ooooooo#",
        ".........#######",
    }},
    {15, 10, 8, 0, 17, { // col_resize
        ".........x.x",
        ".........x.x",
        ".........x.x",
        ".........x.x",
        ".........x.x",
        "...x.....x.x.....x",
        "..xx.....x.x.....xx",
        ".xx......x.x......xx",
        "xxxxxxx..x.x..xxxxxxx",
        ".xx......x.x......xx",
        "..xx.....x.x.....xx",
        "...x.....x.x.....x",
        ".........x.x",
        ".........x.x",
        ".........x.x",
        ".........x.x",
        ".........x.x",
    }},
    {16, 9, 10, 0, 21, { // row_resize
        "........x",
        ".......xxx",
        "......xxxxx",
        ".....xx.x.xx",
        "........x",
        "........x",
        "........x",
        "",
        "",
        "xxxxxxxxxxxxxxxxx",
        "",
        "xxxxxxxxxxxxxxxxx",
        "",
        "",
        "........x",
        "........x",
        "........x",
        ".....xx.x.xx",
        "......xxxxx",
        ".......xxx",
        "........x",
    }},
    {17, 9, 3, 0, 7, { // vertical_text
        "x................x",
        "x................x",
        "x................x",
        ".xxxxxxxxxxxxxxxx",
        "x................x",
        "x................x",
        "x................x",
    }},
    {18, 7, 4, 3, 21, { // copy
        "..................o",
        ".......##........o#o",
        ".......#o#......oo#oo",
        ".......#oo#....o#####o",
        ".......#ooo#....oo#oo",
        ".......#oooo#....o#o",
        ".......#ooooo#....o",
        ".......#oooooo#",
        ".......#ooooooo#",
        ".......#oooooooo#",
        ".......#ooooooooo#",
        ".......#oooooo#####",
        ".......#ooo#oo#",
        ".......#oo##oo#",
        ".......#o#.##oo#",
        ".......##...#oo#",
        ".......#.....#oo#",
        ".............#oo#",
        "..............#oo#",
        "..............#oo#",
        "...............##",
    }},
    {19, 7, 4, 2, 22, { // alias
        "................oooooo",
        "................o####o",
        ".......##........o###o",
        ".......#o#......o####o",
        ".......#oo#....o###o#o",
        ".......#ooo#...o##o.oo",
        ".......#oooo#..o#o",
        ".......#ooooo#..o#o",
        ".......#oooooo#..oo",
        ".......#ooooooo#",
        ".......#oooooooo#",
        ".......#ooooooooo#",
        ".......#oooooo#####",
        ".......#ooo#oo#",
        ".......#oo##oo#",
        ".......#o#.##oo#",
        ".......##...#oo#",
        ".......#.....#oo#",
        ".............#oo#",
        "..............#oo#",
        "..............#oo#",
        "...............##",
    }},
    {20, 7, 4, 4, 20, { // context_menu
        ".......##",
        ".......#o#",
        ".......#oo#",
        ".......#ooo#",
        ".......#oooo#",
        ".......#ooooo#....ooooooooooooo",
        ".......#oooooo#...o###########o",
        ".......#ooooooo#..o#ooooooooo#o",
        ".......#oooooooo#.o#ooooooooo#o",
        ".......#ooooooooo#o#o#######o#o",
        ".......#oooooo####o#ooooooooo#o",
        ".......#ooo#oo#...o#ooooooooo#o",
        ".......#oo##oo#...o#o#######o#o",
        ".......#o#.##oo#..o#ooooooooo#o",
        ".......##...#oo#..o#ooooooooo#o",
        ".......#.....#oo#.o#o#######o#o",
        ".............#oo#.o#ooooooooo#o",
        "..............#oo#o###########o",
        "..............#oo#ooooooooooooo",
        "...............##",
    }},
    {25, 6, 6, 0, 16, { // zoom_in
        "....####",
        "..##oooo##",
        ".#oooooooo#",
        ".#ooo##ooo#",
        "#oooo##oooo#",
        "#oo######oo#",
        "#oo######oo#",
        "#oooo##oooo#",
        ".#ooo##ooo#",
        ".#oooooooo#",
        "..##oooo####",
        "....####..###",
        "...........###",
        "............###",
        ".............###",
        "..............#",
    }},
    {26, 6, 6, 0, 16, { // zoom_out
        "....####",
        "..##oooo##",
        ".#oooooooo#",
        ".#oooooooo#",
        "#oooooooooo#",
        "#oo######oo#",
        "#oo######oo#",
        "#oooooooooo#",
        ".#oooooooo#",
        ".#oooooooo#",
        "..##oooo####",
        "....####..###",
        "...........###",
        "............###",
        ".............###",
        "..............#",
    }},
    {27, 7, 7, 0, 15, { // cell
        "....######",
        "....#oooo##",
        "....#oooo##",
        "....#oooo##",
        "#####oooo#####",
        "#oooooooooooo##",
        "#oooooooooooo##",
        "#oooooooooooo##",
        "#oooooooooooo##",
        "#####oooo######",
        ".####oooo######",
        "....#oooo##",
        "....#oooo##",
        "....#######",
        ".....######",
    }},
    {28, 7, 4, 4, 20, { // no_drop
        ".......##",
        ".......#o#",
        ".......#oo#",
        ".......#ooo#",
        ".......#oooo#",
        ".......#ooooo#",
        ".......#oooooo#",
        ".......#ooooooo#.....ooooooo",
        ".......#oooooooo#...oo#####oo",
        ".......#ooooooooo#.oo#######oo",
        ".......#oooooo####oo###oooo##oo",
        ".......#ooo#oo#...o#####oooo##o",
        ".......#oo##oo#...o##o###ooo##o",
        ".......#o#.##oo#..o##oo###oo##o",
        ".......##...#oo#..o##ooo###o##o",
        ".......#.....#oo#.o##oooo#####o",
        ".............#oo#.oo##oooo###oo",
        "..............#oo#.oo#######oo",
        "..............#oo#..oo#####oo",
        "...............##....ooooooo",
    }},
    {29, 15, 15, 1, 29, { // move
        "..............ooo",
        ".............oo#oo",
        "............oo###oo",
        "...........oo#####oo",
        "..........oo#######oo",
        "..........o#########o",
        "..........ooooo#ooooo",
        "..............o#o",
        "..............o#o",
        ".....ooo......o#o......ooo",
        "....oo#o......o#o......o#oo",
        "...oo##o......o#o......o##oo",
        "..oo###o......o#o......o###oo",
        ".oo####oooooooo#oooooooo####oo",
        ".o###########################o",
        ".oo####oooooooo#oooooooo####oo",
        "..oo###o......o#o......o###oo",
        "...oo##o......o#o......o##oo",
        "....oo#o......o#o......o#oo",
        ".....ooo......o#o......ooo",
        "..............o#o",
        "..............o#o",
        "..........ooooo#ooooo",
        "..........o#########o",
        "..........oo#######oo",
        "...........oo#####oo",
        "............oo###oo",
        ".............oo#oo",
        "..............ooo",
    }},
};

// Cursor `cursor` (types.Cursor) drawn `factor` times larger into `rgba`, which holds
// (32 * factor)^2 RGBA pixels, not premultiplied. False when there is no image for it.
static bool silk_cursor_bundled(uint32_t cursor, uint32_t factor, uint8_t *rgba, uint32_t *hotX,
                                uint32_t *hotY) {
    const SilkCursorArt *art = NULL;
    for (size_t i = 0; i < sizeof silk_cursor_art / sizeof silk_cursor_art[0]; i++)
        if (silk_cursor_art[i].cursor == cursor) art = &silk_cursor_art[i];
    if (!art || factor == 0) return false;
    uint8_t cells[SILK_CURSOR_ART][SILK_CURSOR_ART];
    memset(cells, '.', sizeof cells);
    for (uint32_t row = 0; row < art->count; row++) {
        const char *text = art->rows[row];
        for (uint32_t x = 0; x < SILK_CURSOR_ART && text[x]; x++) cells[art->first + row][x] = (uint8_t)text[x];
    }
    // Inverted pixels become black with a white edge, as they look over a light window.
    for (int y = 0; y < SILK_CURSOR_ART; y++)
        for (int x = 0; x < SILK_CURSOR_ART; x++) {
            if (cells[y][x] != 'x') continue;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < SILK_CURSOR_ART && ny < SILK_CURSOR_ART && cells[ny][nx] == '.')
                        cells[ny][nx] = 'e';
                }
        }
    uint32_t size = SILK_CURSOR_ART * factor;
    for (uint32_t y = 0; y < size; y++)
        for (uint32_t x = 0; x < size; x++) {
            uint8_t cell = cells[y / factor][x / factor];
            uint8_t *pixel = rgba + ((size_t)y * size + x) * 4;
            uint8_t value = (cell == 'o' || cell == 'e') ? 255 : 0;
            bool shown = cell == '#' || cell == 'x' || cell == 'o' || cell == 'e';
            pixel[0] = pixel[1] = pixel[2] = value;
            pixel[3] = shown ? 255 : 0;
        }
    *hotX = art->hotX * factor;
    *hotY = art->hotY * factor;
    return true;
}

#endif
