#!/usr/bin/env python3
"""Generates the game's pixel art into assets/gfx/.

Every sprite is written as text: one character per pixel, looked up in a palette. To change a
sprite, edit its grid here and run `python3 tools/make_art.py`. No drawing program needed.

  '.' is always transparent.
"""
import math
import os
import struct
import zlib

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'gfx')


# ----------------------------------------------------------------------------------------------
# Tiny PNG writer and canvas
# ----------------------------------------------------------------------------------------------

class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[(0, 0, 0, 0)] * w for _ in range(h)]

    def put(self, x, y, rgba):
        if 0 <= x < self.w and 0 <= y < self.h and rgba[3] > 0:
            self.px[y][x] = rgba

    def blit(self, grid, palette, ox, oy):
        for y, row in enumerate(grid):
            for x, ch in enumerate(row):
                if ch != '.':
                    self.put(ox + x, oy + y, palette[ch])

    def save(self, name):
        raw = b''.join(b'\x00' + bytes(c for px in row for c in px) for row in self.px)
        def chunk(tag, data):
            return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', self.w, self.h, 8, 6, 0, 0, 0))
        png += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
        os.makedirs(OUT, exist_ok=True)
        with open(os.path.join(OUT, name), 'wb') as f:
            f.write(png)
        print('wrote', name, f'{self.w}x{self.h}')


def rgb(hex_color, alpha=255):
    hex_color = hex_color.lstrip('#')
    return (int(hex_color[0:2], 16), int(hex_color[2:4], 16), int(hex_color[4:6], 16), alpha)


def rotate(grid):
    """90 degrees clockwise."""
    return [''.join(grid[len(grid) - 1 - x][y] for x in range(len(grid))) for y in range(len(grid[0]))]


def flip_v(grid):
    return list(reversed(grid))


# ----------------------------------------------------------------------------------------------
# Tiles: assets/gfx/tiles.png, 8x8 tiles in a grid 8 wide. The index of each tile is fixed and
# mirrored in src/Level.cpp (namespace `sheet`), so only append new tiles at the end.
# ----------------------------------------------------------------------------------------------

ROCK = {'0': rgb('#221e2e'), '1': rgb('#3a3248'), '2': rgb('#4e4460'), '3': rgb('#6a5c7a'),
        'g': rgb('#2f6a46'), 'G': rgb('#3f9a52'), 'h': rgb('#7cc95a'), 'H': rgb('#b8e67c'),
        'f': rgb('#f2d16b'), 'p': rgb('#e88ab8')}

rock_a = ['11121111',
          '11111101',
          '12111111',
          '11110111',
          '11111121',
          '10111111',
          '11112111',
          '11111110']
rock_b = ['11111111',
          '11011121',
          '11111111',
          '21111101',
          '11121111',
          '11111111',
          '10111211',
          '11111111']
rock_c = ['11111211',
          '12111111',
          '11113211',
          '11112111',
          '11111111',
          '11011111',
          '11111101',
          '21111111']
# Rock that is buried away from any surface: darker, so the level reads as shapes.
rock_deep = [r.replace('1', '0').replace('2', '1') for r in rock_a]

grass_top = ['HhHHhHHh',
             'hGhhGhhG',
             'GgGGgGgG',
             'g1gg1g1g',
             '.g...g..',
             '........',
             '........',
             '........']
grass_top_b = ['hHhHHhHH',
               'GhGhhGhG',
               'gGgGGgGg',
               '1g1gg1g1',
               '..g...g.',
               '........',
               '........',
               '........']
edge_left = ['0.......',
             '02......',
             '0.......',
             '0.......',
             '02......',
             '0.......',
             '0.......',
             '0.......']
edge_right = [r[::-1] for r in edge_left]
edge_bottom = ['........',
               '........',
               '........',
               '........',
               '........',
               '........',
               '........',
               '00000000']
# Little things that grow in the empty tile above grass, picked at random by position.
tuft = ['........',
        '........',
        '........',
        '........',
        '........',
        '....h...',
        '.h..hG.h',
        'hG.GhG.G']
flower = ['........',
          '........',
          '........',
          '.....p..',
          '....pfp.',
          '.....p..',
          '.....G..',
          '....GhG.']
# Hangs into the empty tile below a ceiling.
drip = ['0001000.',
        '.00100..',
        '..010...',
        '..00....',
        '...0....',
        '........',
        '........',
        '........']

SPIKE = {'1': rgb('#eef1f8'), '2': rgb('#9aa2b8'), '3': rgb('#4a4e62')}
CRYSTAL = {'1': rgb('#ffd0e4'), '2': rgb('#e5487a'), '3': rgb('#7a1f45')}
# Pointing up, standing on the bottom of the tile. 5px tall; the hitbox is the bottom 4px.
spikes_up = ['........',
             '........',
             '........',
             '.12..12.',
             '.12..12.',
             '11221122',
             '11221122',
             '33333333']

WOOD = {'0': rgb('#4a2e1e'), '1': rgb('#8a5a34'), '2': rgb('#b47c48'), '3': rgb('#dcaa6c')}
plank_left = ['.3333333',
              '22222222',
              '21121112',
              '.0000000',
              '.00.....',
              '.0......',
              '........',
              '........']
plank_mid = ['33333333',
             '22222222',
             '11211121',
             '00000000',
             '........',
             '........',
             '........',
             '........']
plank_right = [r[::-1] for r in plank_left]

METAL = {'0': rgb('#2a3040'), '1': rgb('#4a566e'), '2': rgb('#6c7c98'), '3': rgb('#a6b6cc'),
         'o': rgb('#e8b94a'), 'O': rgb('#9c6a1e')}
hook_block = ['00000000',
              '03222220',
              '02111110',
              '0211oo10',
              '021oO110',
              '0211oo10',
              '02111110',
              '00000000']

TILES = [
    (rock_a, ROCK), (rock_b, ROCK), (rock_c, ROCK), (rock_deep, ROCK),             # 0-3
    (grass_top, ROCK), (grass_top_b, ROCK), (edge_left, ROCK), (edge_right, ROCK),  # 4-7
    (edge_bottom, ROCK), (tuft, ROCK), (flower, ROCK), (drip, ROCK),                # 8-11
    (spikes_up, SPIKE), (flip_v(spikes_up), SPIKE),                                 # 12-13: up, down
    (rotate(spikes_up), SPIKE), (rotate(rotate(rotate(spikes_up))), SPIKE),         # 14-15: left wall, right wall
    (spikes_up, CRYSTAL), (flip_v(spikes_up), CRYSTAL),                             # 16-17
    (rotate(spikes_up), CRYSTAL), (rotate(rotate(rotate(spikes_up))), CRYSTAL),     # 18-19
    (plank_left, WOOD), (plank_mid, WOOD), (plank_right, WOOD), (hook_block, METAL),  # 20-23
]


def make_tiles():
    cols = 8
    rows = (len(TILES) + cols - 1) // cols
    c = Canvas(cols * 8, rows * 8)
    for i, (grid, pal) in enumerate(TILES):
        c.blit(grid, pal, (i % cols) * 8, (i // cols) * 8)
    c.save('tiles.png')


# ----------------------------------------------------------------------------------------------
# Player: assets/gfx/player.png, 9x12 frames in one row. Order mirrored in src/Player.cpp.
# Based on the original temp_player.png: spiky green/pink hair, cyan band, beak facing right.
# ----------------------------------------------------------------------------------------------

PLAYER = {'a': rgb('#d77bba'), 'b': rgb('#99e550'), 'c': rgb('#5fcde4'), 'd': rgb('#eec39a'),
          'e': rgb('#5b6ee1'), 'f': rgb('#323c39'), 'g': rgb('#fbf236'), 'G': rgb('#d9b52c'),
          'y': rgb('#fffbb0'), 'h': rgb('#000000')}

HEAD = ['.ab.b.ba.',
        '.abbbbba.',
        '.cddeddc.',
        '.cdddedc.',
        '.cddffff.']
HEAD_WIND = ['ab.b.ba..',   # hair blown back while moving fast
             '.abbbbba.',
             '.cddeddc.',
             '.cdddedc.',
             '.cddffff.']
BODY = ['...ggg...',
        '.gyggggg.',
        'gygggggGg',
        'gggggggGg',
        '..GgggG..']


def frame(head, body, feet, drop=0):
    rows = ['.' * 9] * drop + head + body + feet
    return rows[:12] if len(rows) >= 12 else rows + ['.' * 9] * (12 - len(rows))


PLAYER_FRAMES = [
    frame(HEAD, BODY, ['..h...h..', '.hhh.hhh.']),                      # 0 idle
    frame(HEAD, ['.gyggggg.'] + BODY[2:], ['..h...h..', '.hhh.hhh.'], drop=1),  # 1 idle, breathing out
    frame(HEAD, BODY, ['..h..h...', '.hh..hh..']),                      # 2 run
    frame(HEAD, ['.gyggggg.'] + BODY[2:], ['...hh....', '...hhh...'], drop=1),  # 3 run (pass, bob)
    frame(HEAD, BODY, ['...h..h..', '..hh..hh.']),                      # 4 run
    frame(HEAD, ['.gyggggg.'] + BODY[2:], ['....hh...', '...hhh...'], drop=1),  # 5 run (pass, bob)
    frame(HEAD, BODY, ['..hh.hh..', '.........']),                      # 6 jump (feet tucked)
    frame(HEAD_WIND, BODY, ['..h...h..', '..h...h..']),                 # 7 fall
    frame(HEAD_WIND, BODY, ['...hhh...', '...h.h...']),                 # 8 swing
]


def make_player():
    c = Canvas(9 * len(PLAYER_FRAMES), 12)
    for i, f in enumerate(PLAYER_FRAMES):
        assert len(f) == 12 and all(len(r) == 9 for r in f), f'player frame {i} is not 9x12'
        c.blit(f, PLAYER, i * 9, 0)
    c.save('player.png')


# ----------------------------------------------------------------------------------------------
# Objects: assets/gfx/objects.png, 8x24 frames. Flags stand on the bottom of the frame.
# 0 checkpoint off, 1-2 checkpoint on (waving), 3-5 goal (waving).
# ----------------------------------------------------------------------------------------------

FLAG = {'|': rgb('#c8c8d4'), '-': rgb('#8a8a9a'),
        'n': rgb('#6e6e86'), 'N': rgb('#9090a8'),          # unlit
        'k': rgb('#3fae55'), 'K': rgb('#7ef08e'),          # lit
        'o': rgb('#e0a030'), 'O': rgb('#ffd85a'), 'W': rgb('#fff6c8')}  # goal


def flag(cloth, height=14):
    rows = ['........'] * 24
    for y in range(24 - height, 24):
        rows[y] = '.|......'
    rows[23] = '-|-.....'
    for i, line in enumerate(cloth):
        y = 24 - height + i
        rows[y] = '.|' + line + '.' * (6 - len(line))
    return rows


cp_off = flag(['nN', 'nNN', 'nNn', 'nN', 'n'])
cp_on1 = flag(['KKKk', 'KKKKk', 'kKKKk', 'KKKk'])
cp_on2 = flag(['KKk', 'KKKKk', 'KKKKKk', 'kKKk'])
goal_1 = flag(['WOOOO', 'OOOOOo', 'OOOOOo', 'OOOOo', 'oOOo'], height=22)
goal_2 = flag(['WOOOo', 'OOOOOO', 'OOOOOo', 'oOOOOo', 'oOo'], height=22)
goal_3 = flag(['WOOO', 'OOOOOo', 'OOOOOO', 'OOOOOo', 'ooO'], height=22)
OBJECTS = [cp_off, cp_on1, cp_on2, goal_1, goal_2, goal_3]


def make_objects():
    c = Canvas(8 * len(OBJECTS), 24)
    for i, f in enumerate(OBJECTS):
        c.blit(f, FLAG, i * 8, 0)
    c.save('objects.png')


# ----------------------------------------------------------------------------------------------
# Backgrounds: 320x180, tile horizontally. Drawn behind the level with parallax.
# ----------------------------------------------------------------------------------------------

def periodic_noise(x, seed, octaves=((1, 1.0), (3, 0.5), (7, 0.25), (13, 0.12))):
    """Smooth noise that repeats every 320 px, so the layer tiles seamlessly."""
    v = 0.0
    for freq, amp in octaves:
        v += amp * math.sin(2 * math.pi * freq * x / 320.0 + seed * (freq + 1.7))
    return v


def make_background(name, color, top_depth, bottom_height, seed, jag):
    c = Canvas(320, 180)
    col = rgb(color)
    for x in range(320):
        top = int(top_depth + 18 * periodic_noise(x, seed) + jag * ((x * 7919 + seed * 13) % 5 - 2))
        bottom = int(bottom_height + 16 * periodic_noise(x, seed + 4.2) + jag * ((x * 104729 + seed) % 5 - 2))
        for y in range(0, max(0, top)):
            c.put(x, y, col)
        for y in range(180 - max(0, bottom), 180):
            c.put(x, y, col)
    c.save(name)


if __name__ == '__main__':
    make_tiles()
    make_player()
    make_objects()
    make_background('bg_far.png', '#232638', top_depth=38, bottom_height=44, seed=1.3, jag=1)
    make_background('bg_near.png', '#1a1c2a', top_depth=22, bottom_height=28, seed=5.1, jag=2)
