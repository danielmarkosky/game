#!/usr/bin/env python3
"""Generates the game's pixel art into assets/gfx/.

Every sprite is written as text: one character per pixel, looked up in a palette. To change a
sprite, edit its grid here and run `python3 tools/make_art.py`. No drawing program needed.

  '.' is always transparent.
"""
import math
import os
import random
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

    def mix(self, x, y, rgba):
        """Like put, but blends a see-through colour over what's already there."""
        if 0 <= x < self.w and 0 <= y < self.h:
            a = rgba[3] / 255
            old = self.px[y][x]
            self.px[y][x] = tuple(round(old[i] * (1 - a) + rgba[i] * a) for i in range(3)) + (max(old[3], rgba[3]),)

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
        path = os.path.join(OUT, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as f:
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
# Tiles: assets/gfx/themes/<theme>/tiles.png, 8x8 tiles in a grid 8 wide. The index of each tile
# is fixed and mirrored in src/Level.cpp (namespace `sheet`), so only append new tiles at the end.
# These are the cave's; the other themes (further down) swap in their own colours and decorations.
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

# A portcullis: solid until a lever opens it.
gate = ['.1..1..1',
        '.2..2..2',
        '33333333',
        '.2..2..2',
        '.2..2..2',
        '33333333',
        '.2..2..2',
        '.1..1..1']


# Slick stone, polished smooth by the wind: solid, but the rope slides off it. The rock of its
# theme, only smooth, with a sheen running across it: you can tell, if you look.
slick = ['33333332',
         '3222222w',
         '322222w1',
         '32222w21',
         '3222w221',
         '322w2221',
         '32w22221',
         '21111111']


def slick_palette(rock):
    """The theme's rock, a shade lighter and polished: a soft sheen, not a white block."""
    def lighter(c, t):
        return tuple(round(c[i] + (255 - c[i]) * t) for i in range(3)) + (255,)
    return {'1': lighter(rock['1'], 0.08), '2': lighter(rock['2'], 0.12), '3': lighter(rock['3'], 0.2), 'w': lighter(rock['3'], 0.45)}


# Ice: solid and grabbable by the rope, but you slide on it and hands don't hold on it.
ICE = {'1': rgb('#5a8ab8'), '2': rgb('#9ad0f0'), '3': rgb('#d8f4ff'), 'w': rgb('#ffffff')}
ice_tile = ['33333333',
            '3w222232',
            '322w2221',
            '3222w221',
            '32222w21',
            '3222222w',
            '32w22221',
            '21111111']

# Crumbling stone: cracked, it gives way a moment after you stand on it, and comes back.
CRUMBLE = {'0': rgb('#3a2c24'), '1': rgb('#7a6250'), '2': rgb('#a8886c'), '3': rgb('#cfb090')}
crumble_tile = ['33332333',
                '32220221',
                '322022.1',
                '3.20.221',
                '32220221',
                '322.0221',
                '3222.021',
                '11011101']

# Cloud: stand on it, and it fades from under you after a moment.
CLOUD = {'1': rgb('#b8c8e8'), '2': rgb('#e0eaff'), '3': rgb('#ffffff')}
cloud_tile = ['.33.333.',
              '33333333',
              '32232223',
              '22222222',
              '.112211.',
              '........',
              '........',
              '........']


# A bounce mushroom's cap: soft, springy, spotted. Land on it and up you go.
BOUNCE = {'1': rgb('#7a2a4e'), '2': rgb('#c44a74'), '3': rgb('#e87a9e'), 'w': rgb('#ffe2ec')}
bounce_tile = ['.333333.',
               '33w23333',
               '3www2223',
               '32w22w23',
               '2222www2',
               '22222w22',
               '12222221',
               '11111111']

# Rails for the mine carts, on wooden sleepers: a one-way platform.
RAIL = {'2': rgb('#6a7080'), '3': rgb('#aab2c0'), 'w': rgb('#7a5030'), 'W': rgb('#4a2e1e')}
rail_tile = ['33333333',
             '22222222',
             'WwwWWwwW',
             '.W....W.',
             '.W....W.',
             '........',
             '........',
             '........']


def tile_sheet(rock, grass=(grass_top, grass_top_b), decor=(tuft, flower, drip), spike=SPIKE,
               rocks=(rock_a, rock_b, rock_c, rock_deep)):
    """The tiles of one theme: `rock` is the palette for the rock and everything growing on it."""
    return [
        (rocks[0], rock), (rocks[1], rock), (rocks[2], rock), (rocks[3], rock),         # 0-3
        (grass[0], rock), (grass[1], rock), (edge_left, rock), (edge_right, rock),      # 4-7
        (edge_bottom, rock), (decor[0], rock), (decor[1], rock), (decor[2], rock),      # 8-11: tuft, flower, drip
        (spikes_up, spike), (flip_v(spikes_up), spike),                                 # 12-13: up, down
        (rotate(spikes_up), spike), (rotate(rotate(rotate(spikes_up))), spike),         # 14-15: left wall, right wall
        (spikes_up, CRYSTAL), (flip_v(spikes_up), CRYSTAL),                             # 16-17
        (rotate(spikes_up), CRYSTAL), (rotate(rotate(rotate(spikes_up))), CRYSTAL),     # 18-19
        (plank_left, WOOD), (plank_mid, WOOD), (plank_right, WOOD), (hook_block, METAL),  # 20-23
        (gate, METAL), (slick, slick_palette(rock)), (ice_tile, ICE), (crumble_tile, CRUMBLE),    # 24-27
        (cloud_tile, CLOUD), (bounce_tile, BOUNCE), (rail_tile, RAIL),                  # 28-30
    ]


def make_tiles(name, tiles):
    cols = 8
    rows = (len(tiles) + cols - 1) // cols
    c = Canvas(cols * 8, rows * 8)
    for i, (grid, pal) in enumerate(tiles):
        c.blit(grid, pal, (i % cols) * 8, (i // cols) * 8)
    c.save(f'themes/{name}/tiles.png')


# ----------------------------------------------------------------------------------------------
# Player: assets/gfx/player.png, one row per look (the heroine, then the people you meet), 12x14
# frames. The order of both is mirrored in src/Player.h and .cpp: idle (2), run (4), jump, fall,
# swing, sitting, climbing (2, facing the wall, hands up on it).
# ----------------------------------------------------------------------------------------------

FRAME_W, FRAME_H = 12, 14
OUTLINE = rgb('#16131f')

# The heroine: you. Long black hair with a red ribbon in it, a teal dress with a white collar and a
# gold clasp. Drawn once, standing, as a 10x12 fill facing right: `head` rows on top of `body` rows
# (10 rows together), then 2 rows of legs drawn by the code below in her leg (L) and foot (F)
# colours. Every pose is made from that, and outlined automatically. `wind_head` and `wind_body`
# are the same with her hair streaming out behind her, for running, falling and swinging.
HEROINE = dict(
    name='heroine',
    palette={'h': '#15121f', 'H': '#2e2a48', 'i': '#6466a0', 'd': '#f8dcc4', 'D': '#dba38c', 'e': '#2a1f45',
             'p': '#f0909c', 'r': '#e0455a', 'R': '#a02840', 'c': '#2fb3a6', 'C': '#1d7a7c', 'g': '#f2c95a',
             'n': '#3a2f52', 'k': '#6b4430', 'w': '#f4efe4'},
    head=['.r.hhhhh..',
          'rRhHiihhh.',
          '.hHhhhhhhh',
          '.hhhdhddhh',
          '.hhdeddedh',
          '.hhpddddph'],
    body=['.hhhDdDhh.',
          '.hhcwwwch.',
          '.hdcCgccd.',
          '..cccccC..'],
    wind_head=['.r.hhhhh..',
               'rRhHiihhh.',
               'hhHhhhhhhh',
               'hhhhdhddhh',
               '.hhdeddedh',
               '..hpddddph'],
    wind_body=['..hhDdDhh.',
               '..hcwwwch.',
               '..dcCgccd.',
               '..cccccC..'],
    legs='nk', hand='d')

# Legs for each pose, in leg (L) and foot (F) colours. A '-' row means the body bobs down a pixel.
LEGS = {
    'idle': ['..LL.LL...', '..FF.FF...'],
    'run_a': ['.LL...LL..', 'FF.....FF.'],
    'pass_a': ['-', '...FFF....'],
    'run_b': ['..LL..LL..', '.FF....FF.'],
    'pass_b': ['-', '....FFF...'],
    'jump': ['..LL.LL...', '..........'],
    'fall': ['..L...L...', '..F...F...'],
    'swing': ['...LL.....', '...FF.....'],
    'sit': ['-', '..LLLLFF..'],
    'climb_a': ['...LL.L...', '...F...F..'],
    'climb_b': ['..L.LL....', '..F..F....'],
}


def pose(skin, legs, breathe=False, hand_up=False, wind=False):
    head, body = list(skin['head']), list(skin['body'])
    if wind and 'wind_head' in skin:  # hair streaming out behind
        head, body = list(skin['wind_head']), list(skin['wind_body'])
    assert len(head) + len(body) == 10 and all(len(r) == 10 for r in head + body), skin['name']
    if breathe:  # the head sinks a pixel into the body
        head, body = ['.' * 10] + head, body[1:]
    if hand_up:  # an arm up in front of the face, holding the rope
        for row in (len(skin['head']) - 2, len(skin['head']) - 1):
            head[row] = head[row][:9] + skin['hand']
    rows = LEGS[legs]
    if rows[0] == '-':
        upper, legs_rows = ['.' * 10] + head + body, rows[1:]
    else:
        upper, legs_rows = head + body, rows
    legs_rows = [r.replace('L', skin['legs'][0]).replace('F', skin['legs'][1]) for r in legs_rows]
    return upper + legs_rows


def outlined(fill):
    """Puts a 10x12 fill in the middle of a frame and draws a 1px outline round it."""
    g = ['.' * FRAME_W] + ['.' + r + '.' for r in fill] + ['.' * FRAME_W]
    out = [list(r) for r in g]
    for y in range(FRAME_H):
        for x in range(FRAME_W):
            if g[y][x] == '.' and any(0 <= x + dx < FRAME_W and 0 <= y + dy < FRAME_H and g[y + dy][x + dx] != '.'
                                      for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[y][x] = '#'
    return [''.join(r) for r in out]


def skin_frames(skin):
    poses = [pose(skin, 'idle'), pose(skin, 'idle', breathe=True),
             pose(skin, 'run_a', wind=True), pose(skin, 'pass_a', wind=True), pose(skin, 'run_b', wind=True),
             pose(skin, 'pass_b', wind=True), pose(skin, 'jump'), pose(skin, 'fall', wind=True),
             pose(skin, 'swing', hand_up=True, wind=True), pose(skin, 'sit'),
             pose(skin, 'climb_a', hand_up=True), pose(skin, 'climb_b', hand_up=True)]
    return [outlined(p) for p in poses]


# The people you meet: drawn like the heroine, and after her in player.png.
# Their names are what an Npc's `look` field says, and mirrored in Player::LOOKS.
NPCS = [
    dict(name='odo',  # the road mender at the cave's mouth: a straw hat, a grey beard, a work shirt, a tool belt
         palette={'y': '#e8c870', 'Y': '#b8944a', 'd': '#e0a878', 'D': '#b07850', 'e': '#16131f', 'b': '#b8b8c0',
                  's': '#4a7ab0', 'S': '#2f5582', 'l': '#7a4e2c', 'k': '#4a3a30'},
         head=['...yyy....',
               '..yYYYy...',
               'yyyyyyyyy.',
               '..dddded..',
               '..bbbbbb..',
               '...bbbb...'],
         body=['.ssssss...',
               'dsSssssd..',
               '.llllll...',
               '..ssssS...'],
         legs='kk', hand='d'),
    dict(name='fen',  # by the campfire: a deep blue hooded cloak sewn with little stars, a glowing pendant
         palette={'c': '#3a3a8a', 'C': '#26266a', 'v': '#f0e8a0', 'd': '#e8c09a', 'D': '#b88a68', 'e': '#16131f',
                  'g': '#7af0e0', 'k': '#2a2440'},
         head=['...cccc...',
               '..ccvccc..',
               '.ccddddcc.',
               '.ccdddedc.',
               '.ccddddcc.',
               '..cDddDc..'],
         body=['.cccgccc..',
               'dcvcccccd.',
               '.ccccvcc..',
               '.cCcccCc..'],
         legs='kk', hand='d'),
    dict(name='mags',  # keeps the stall by the campfire: a wide purple hat with a red feather, spectacles, a patchwork shawl
         palette={'h': '#6a3a7a', 'H': '#e8b94a', 'f': '#e8445a', 'd': '#e8b890', 'D': '#b88a68', 'g': '#c8d8f0',
                  'w': '#e8e4ec', 's': '#c86a3a', 'S': '#3a8a8a', 'p': '#e8c04a', 'k': '#3a2a40'},
         head=['.....f....',
               '..hhhhf...',
               'hhHHHHHhh.',
               '..wddddw..',
               '..wgdgdw..',
               '...dDdd...'],
         body=['.sSsSsSs..',
               'dsSpSsSsd.',
               '.SsSsSpS..',
               '..sSsSs...'],
         legs='kk', hand='d'),
]


# You, in the other outfits: the heroine's drawing with her clothes changed. Their names are
# what assets/outfits.txt calls them, and mirrored in Player::LOOKS.
def _dressed(name, palette, head=None, body=None, wind=True, legs='nk'):
    look = dict(HEROINE, name=name, palette=dict(HEROINE['palette'], **palette), legs=legs)
    if head:
        look['head'] = head
    if body:
        look['body'] = body
    if not wind or head:  # a hood or a helmet doesn't stream out behind you
        look.pop('wind_head', None)
        look.pop('wind_body', None)
    return look


OUTFITS = [
    # A green hooded cloak, a leaf clasp.
    _dressed('ranger', {'v': '#2f6a40', 'V': '#1f4a2c', 'c': '#3f8a50', 'C': '#2a6a3a', 'g': '#a8d46a', 'w': '#8a6a40', 'n': '#4a3a2a'},
             head=['..vvvvv...',
                   '.vVVVVVv..',
                   'vVhhhhhVv.',
                   'vVhdhddhv.',
                   'vhhdeddev.',
                   '.vhpddddp.'],
             body=['.vvhDdDvv.',
                   '.vvcwwwcv.',
                   '.vdcCgccd.',
                   '..cccccC..']),
    # The diving suit: a brass helmet with a round window, a grey suit, an air tank on the back.
    _dressed('diver', {'y': '#d8a84a', 'Y': '#8a6420', 'G': '#9ad0e8', 'c': '#5a6a7a', 'C': '#3a4656', 'k': '#2a2a34',
                       't': '#c8c8d0', 'n': '#3a4656'},
             head=['..yyyyy...',
                   '.yYYYYYy..',
                   'yYGGGGGYy.',
                   'yYGdedGYy.',
                   'yYGGdGGYy.',
                   '.yyyyyyy..'],
             body=['tccyyycc..',
                   'tcccccccc.',
                   'tdcCcccd..',
                   '..cccccC..'], legs='nk'),
    # A fox hood, ears and all, and an orange coat.
    _dressed('fox', {'o': '#e8823a', 'O': '#a8501e', 'w': '#f4ecdc', 'c': '#e8823a', 'C': '#a8501e', 'g': '#f4ecdc'},
             head=['.o.....o..',
                   '.oO...Oo..',
                   '.ooooooo..',
                   'oohdhddho.',
                   'owhdeddew.',
                   '.wwpddddp.'],
             body=['.ooowwwoo.',
                   '.oowwwwwo.',
                   '.dcCgccd..',
                   '..cccccC..']),
    # The miner's hat: a yellow helmet with a lamp on the front, and brown overalls.
    _dressed('miner', {'y': '#e8c84a', 'Y': '#a8862a', 'L': '#fff8c0', 'c': '#7a5a3a', 'C': '#5a3e26', 'g': '#c8c8d0', 'w': '#c8b8a0'},
             head=['...yyyL...',
                   '..yYyyLL..',
                   '.yyyyyyyy.',
                   '.hhhdhddhh',
                   '.hhdeddedh',
                   '.hhpddddph']),
    # Starlight: a dress of night blue, sewn with stars.
    _dressed('starlight', {'c': '#2a2a6a', 'C': '#1a1a4a', 'w': '#f0e8a0', 'g': '#fff8d0', 'r': '#f0e8a0', 'R': '#a8a060'},
             body=['.hhhDdDhh.',
                   '.hhcwcwch.',
                   '.hdcCgcwd.',
                   '..cwcccC..'],
             wind=False),
]


# Pip, your dog: a little tan terrier with a red collar. Not drawn like the people: his twelve
# frames are here as they are, in the same slots (idle 2, run 4, jump, fall, then pointing his
# nose where the swing would be, sitting, and barking where climbing would be).
PIP = dict(name='pip', palette={'o': '#d8a060', 'O': '#9a6a3a', 'w': '#f4ecdc', 'e': '#16131f', 'n': '#3a2a2a',
                                 'c': '#e0455a', 'm': '#a02840'})
_pip_top = ['..........'] * 5
_pip_up = ['..........'] * 4
PIP_FRAMES = [
    _pip_top + ['......O.O.', '......oooo', 'O.....oeon', '.ooooocoo.', '.oooooww..', '.oo..oo...', '.OO..OO...'],   # idle
    _pip_top + ['......O.O.', '......oooo', '......oeon', 'Oooooocoo.', '.oooooww..', '.oo..oo...', '.OO..OO...'],   # idle, wagging
    _pip_top + ['......O.O.', '......oooo', 'O.....oeon', '.ooooocoo.', '.oooooww..', 'o.....o.o.', 'O......O..'],   # run
    _pip_up + ['......O.O.', '......oooo', 'O.....oeon', '.ooooocoo.', '.oooooww..', '..oooo....', '..OO.O....', '..........'],
    _pip_top + ['......O.O.', '......oooo', '.O....oeon', '.ooooocoo.', '.oooooww..', 'o.....o.o.', 'O......O..'],
    _pip_up + ['......O.O.', '......oooo', '.O....oeon', '.ooooocoo.', '.oooooww..', '..oooo....', '..OO.O....', '..........'],
    _pip_up + ['......O.O.', '......oooo', 'O.....oeon', '.ooooocoo.', '.oooooww..', '..oo.oo...', '..........', '..........'],  # jump
    _pip_top + ['......O.O.', '......oooo', '......oeon', 'Oooooocoo.', '.oooooww..', '.o.....o..', '.O.....O..'],   # fall
    _pip_top + ['.......O.O', '.......ooo', 'OO.....oeo', '.ooooocoon', '.oooooww..', '.oo...o.o.', '.OO....O..'],  # pointing
    _pip_up + ['......O.O.', '......oooo', '......oeon', '.....ocoo.', 'O...ooww..', '.oooooww..', '.ooooo.o..', '.OOOO.OO..'],  # sitting
    _pip_top + ['......O.O.', '......oooo', 'O.....oeon', '.ooooocomm', '.oooooww..', '.oo..oo...', '.OO..OO...'],  # barking
    _pip_top + ['.....O.O..', '.....oooo.', '.O...oeon.', '.oooocoo..', '.ooooww...', '.oo..oo...', '.OO..OO...'],
]


def make_player():
    rows = []
    for skin in [HEROINE] + NPCS:
        palette = {k: rgb(v) for k, v in skin['palette'].items()}
        palette['#'] = OUTLINE
        rows.append((skin['name'], palette, skin_frames(skin)))
    palette = {k: rgb(v) for k, v in PIP['palette'].items()}
    palette['#'] = OUTLINE
    rows.append(('pip', palette, [outlined(f) for f in PIP_FRAMES]))
    for outfit in OUTFITS:
        palette = {k: rgb(v) for k, v in outfit['palette'].items()}
        palette['#'] = OUTLINE
        rows.append((outfit['name'], palette, skin_frames(outfit)))
    c = Canvas(FRAME_W * 12, FRAME_H * len(rows))
    for r, (name, palette, frames) in enumerate(rows):
        assert len(frames) == 12, name
        for i, f in enumerate(frames):
            assert len(f) == FRAME_H and all(len(row) == FRAME_W for row in f), f'{name} frame {i} is not {FRAME_W}x{FRAME_H}'
            c.blit(f, palette, i * FRAME_W, r * FRAME_H)
    c.save('player.png')
    print('  looks:', ', '.join(name for name, _, _ in rows))


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
# Items: assets/gfx/items.png, 10x10 frames, things to pick up that give an ability. The order is
# mirrored in src/Level.cpp (namespace `sheet`). 0: the rope, 1: a gem, 2: the long rope,
# 3-4: a lever up and pulled down, 5: the updraft feather, 6: the chalk bag, 7: the throwing hand,
# 8: the stopwatch.
# ----------------------------------------------------------------------------------------------

ITEM = {'3': rgb('#e8eef8'), '2': rgb('#9aa6bc'), 'w': rgb('#b47c48'), 'W': rgb('#7a4e2c'), '#': OUTLINE,
        'a': rgb('#eaffff'), 'b': rgb('#9ff0ff'), 'c': rgb('#4fd0e8'), 'd': rgb('#2a8fb0'),
        'r': rgb('#e6d6b4'), 'R': rgb('#9a8460'), 'k': rgb('#e8443a'), 'K': rgb('#9a2a2a'),
        'f': rgb('#ffffff'), 'F': rgb('#bfe6ff'), 'q': rgb('#7ab8e8'),
        'g': rgb('#4ac08a'), 'G': rgb('#2a8a5a'), 'l': rgb('#c89a6a'), 'L': rgb('#8a5e3a'),
        'y': rgb('#e8c84a'), 'Y': rgb('#a8862a')}
# The rope Odo gives you: a coil like the long rope, with a green thread woven through it.
rope = ['..rgrr..',
        '.rRRGRr.',
        'gR.rr.Rr',
        'rR.RG.Rg',
        'rG.rr.Rr',
        '.rRRgRr.',
        '..rrgrR.',
        '.......g']


def item_frame(grid):
    """Pads an 8x8 drawing to 10x10 with an outline round it."""
    g = ['.' * 10] + ['.' + r + '.' for r in grid] + ['.' * 10]
    out = [list(r) for r in g]
    for y in range(10):
        for x in range(10):
            if g[y][x] == '.' and any(0 <= x + dx < 10 and 0 <= y + dy < 10 and g[y + dy][x + dx] != '.'
                                      for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[y][x] = '#'
    return [''.join(r) for r in out]


# Cut like a diamond: flat on top, pointed below, lit from the top left.
gem = ['........',
       '..abbc..',
       '.abbccd.',
       'abbcccdd',
       '.bccccd.',
       '..cccd..',
       '...cd...',
       '........']

# A coil of rope, its loose end trailing.
long_rope = ['..rrrr..',
             '.rRRRRr.',
             'rR.rr.Rr',
             'rR.RR.Rr',
             'rR.rr.Rr',
             '.rRRRRr.',
             '..rrrrR.',
             '.......r']

# A lever on a wooden base: up, then pulled down.
lever_up = ['.kk.....',
            '.kK.....',
            '..2.....',
            '...2....',
            '....2...',
            '.....2..',
            '..WWWWW.',
            '..WwwwW.']
lever_down = [row[::-1] for row in lever_up[:6]] + lever_up[6:]
# A white feather, lifting.
feather = ['......fF',
           '.....fFq',
           '....fFq.',
           '...fFq..',
           '..fFq...',
           '.fFq....',
           '.Fq.....',
           'W.......']

# A chalk bag: a pouch on a drawstring, white with chalk at the top.
chalk_bag = ['...LL...',
             '..L..L..',
             '.ffffff.',
             '.lFffFl.',
             '.llllll.',
             '.lLllLl.',
             '.llllll.',
             '..LLLL..']
# The throwing hand: a leather glove, fingers up, a loop of rope round the wrist.
throwing_hand = ['.l.l.l..',
                 '.l.l.l..',
                 '.llllll.',
                 '.lllllL.',
                 '.llllL..',
                 '..lllL..',
                 '.rRrRr..',
                 '..rRr...']

# Gran's stopwatch: brass, on a loop of ribbon.
stopwatch = ['...yy...',
             '..YyyY..',
             '.y3333y.',
             'y33233fy',
             'y33233fy',
             'y33322fy',
             '.y3333y.',
             '..yyyy..']

ITEMS = [rope, gem, long_rope, lever_up, lever_down, feather, chalk_bag, throwing_hand, stopwatch]


def make_items():
    c = Canvas(10 * len(ITEMS), 10)
    for i, grid in enumerate(ITEMS):
        c.blit(item_frame(grid), ITEM, i * 10, 0)
    c.save('items.png')


# ----------------------------------------------------------------------------------------------
# Bench: assets/gfx/bench.png, where the game is saved and travelled from. Two tiles wide,
# outlined.
# ----------------------------------------------------------------------------------------------

BENCH = {'0': rgb('#4a2e1e'), '1': rgb('#8a5a34'), '2': rgb('#b47c48'), '3': rgb('#dcaa6c'), '#': OUTLINE}
bench = ['.33333333333333.',
         '.22222222222222.',
         '..1..........1..',
         '..1..........1..',
         '3333333333333333',
         '2222222222222222',
         '.01..........10.',
         '.01..........10.']


def outlined_grid(grid):
    """Pads a drawing by a pixel all round and outlines it."""
    w, h = len(grid[0]) + 2, len(grid) + 2
    g = ['.' * w] + ['.' + r + '.' for r in grid] + ['.' * w]
    out = [list(r) for r in g]
    for y in range(h):
        for x in range(w):
            if g[y][x] == '.' and any(0 <= x + dx < w and 0 <= y + dy < h and g[y + dy][x + dx] != '.'
                                      for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[y][x] = '#'
    return [''.join(r) for r in out]


def make_bench():
    grid = outlined_grid(bench)
    c = Canvas(len(grid[0]), len(grid))
    c.blit(grid, BENCH, 0, 0)
    c.save('bench.png')


# ----------------------------------------------------------------------------------------------
# Font: assets/gfx/font.png, the text of conversations. Printable ASCII from ' ' to '~', 16 to a
# row, each in a 6x9 cell, drawn white so the game can tint it. Capitals are 7 px tall, small
# letters 5, and g j p q y hang 2 px below the line. The game measures each glyph's width from
# its pixels, so glyphs start at the left of their cell and a narrow one is just narrow.
# ----------------------------------------------------------------------------------------------

FONT_CELL_W, FONT_CELL_H = 6, 9


def glyph(rows, top=0):
    """A glyph whose first row is `top` rows down from the top of the capitals."""
    return [''] * top + rows


FONT = {
    ' ': glyph([]),
    '!': glyph(['#', '#', '#', '#', '#', '.', '#']),
    '"': glyph(['#.#', '#.#']),
    '#': glyph(['.#.#.', '#####', '.#.#.', '#####', '.#.#.'], 1),
    '$': glyph(['.###.', '#.#..', '.###.', '..#.#', '.###.'], 1),
    '%': glyph(['##..#', '##.#.', '..#..', '.#.##', '#..##'], 1),
    '&': glyph(['.##..', '#..#.', '.##..', '#.#.#', '#..#.', '.##.#'], 1),
    "'": glyph(['#', '#']),
    '(': glyph(['.#', '#.', '#.', '#.', '#.', '#.', '.#']),
    ')': glyph(['#.', '.#', '.#', '.#', '.#', '.#', '#.']),
    '*': glyph(['#.#', '.#.', '#.#'], 2),
    '+': glyph(['.#.', '###', '.#.'], 3),
    ',': glyph(['.#', '.#', '#.'], 6),
    '-': glyph(['###'], 4),
    '.': glyph(['#'], 6),
    '/': glyph(['..#', '..#', '.#.', '.#.', '#..', '#..'], 1),
    '0': glyph(['.###.', '#...#', '#..##', '#.#.#', '##..#', '#...#', '.###.']),
    '1': glyph(['.#.', '##.', '.#.', '.#.', '.#.', '.#.', '###']),
    '2': glyph(['.###.', '#...#', '....#', '...#.', '..#..', '.#...', '#####']),
    '3': glyph(['#####', '...#.', '..#..', '...#.', '....#', '#...#', '.###.']),
    '4': glyph(['...#.', '..##.', '.#.#.', '#..#.', '#####', '...#.', '...#.']),
    '5': glyph(['#####', '#....', '####.', '....#', '....#', '#...#', '.###.']),
    '6': glyph(['..##.', '.#...', '#....', '####.', '#...#', '#...#', '.###.']),
    '7': glyph(['#####', '....#', '...#.', '..#..', '.#...', '.#...', '.#...']),
    '8': glyph(['.###.', '#...#', '#...#', '.###.', '#...#', '#...#', '.###.']),
    '9': glyph(['.###.', '#...#', '#...#', '.####', '....#', '...#.', '.##..']),
    ':': glyph(['#', '.', '.', '#'], 3),
    ';': glyph(['.#', '..', '..', '.#', '.#', '#.'], 3),
    '<': glyph(['..#', '.#.', '#..', '.#.', '..#'], 1),
    '=': glyph(['###', '...', '###'], 3),
    '>': glyph(['#..', '.#.', '..#', '.#.', '#..'], 1),
    '?': glyph(['.###.', '#...#', '....#', '...#.', '..#..', '.....', '..#..']),
    '@': glyph(['.###.', '#...#', '#.###', '#.#.#', '#.###', '#....', '.###.']),
    'A': glyph(['.###.', '#...#', '#...#', '#####', '#...#', '#...#', '#...#']),
    'B': glyph(['####.', '#...#', '#...#', '####.', '#...#', '#...#', '####.']),
    'C': glyph(['.###.', '#...#', '#....', '#....', '#....', '#...#', '.###.']),
    'D': glyph(['####.', '#...#', '#...#', '#...#', '#...#', '#...#', '####.']),
    'E': glyph(['#####', '#....', '#....', '####.', '#....', '#....', '#####']),
    'F': glyph(['#####', '#....', '#....', '####.', '#....', '#....', '#....']),
    'G': glyph(['.###.', '#...#', '#....', '#.###', '#...#', '#...#', '.###.']),
    'H': glyph(['#...#', '#...#', '#...#', '#####', '#...#', '#...#', '#...#']),
    'I': glyph(['###', '.#.', '.#.', '.#.', '.#.', '.#.', '###']),
    'J': glyph(['...#', '...#', '...#', '...#', '#..#', '#..#', '.##.']),
    'K': glyph(['#...#', '#..#.', '#.#..', '##...', '#.#..', '#..#.', '#...#']),
    'L': glyph(['#...', '#...', '#...', '#...', '#...', '#...', '####']),
    'M': glyph(['#...#', '##.##', '#.#.#', '#.#.#', '#...#', '#...#', '#...#']),
    'N': glyph(['#...#', '##..#', '#.#.#', '#..##', '#...#', '#...#', '#...#']),
    'O': glyph(['.###.', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.']),
    'P': glyph(['####.', '#...#', '#...#', '####.', '#....', '#....', '#....']),
    'Q': glyph(['.###.', '#...#', '#...#', '#...#', '#.#.#', '#..#.', '.##.#']),
    'R': glyph(['####.', '#...#', '#...#', '####.', '#.#..', '#..#.', '#...#']),
    'S': glyph(['.###.', '#...#', '#....', '.###.', '....#', '#...#', '.###.']),
    'T': glyph(['#####', '..#..', '..#..', '..#..', '..#..', '..#..', '..#..']),
    'U': glyph(['#...#', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.']),
    'V': glyph(['#...#', '#...#', '#...#', '#...#', '#...#', '.#.#.', '..#..']),
    'W': glyph(['#...#', '#...#', '#...#', '#.#.#', '#.#.#', '##.##', '#...#']),
    'X': glyph(['#...#', '#...#', '.#.#.', '..#..', '.#.#.', '#...#', '#...#']),
    'Y': glyph(['#...#', '#...#', '.#.#.', '..#..', '..#..', '..#..', '..#..']),
    'Z': glyph(['#####', '....#', '...#.', '..#..', '.#...', '#....', '#####']),
    '[': glyph(['##', '#.', '#.', '#.', '#.', '#.', '##']),
    '\\': glyph(['#..', '#..', '.#.', '.#.', '..#', '..#'], 1),
    ']': glyph(['##', '.#', '.#', '.#', '.#', '.#', '##']),
    '^': glyph(['.#.', '#.#']),
    '_': glyph(['#####'], 7),
    '`': glyph(['#.', '.#']),
    'a': glyph(['.###.', '....#', '.####', '#...#', '.####'], 2),
    'b': glyph(['#....', '#....', '####.', '#...#', '#...#', '#...#', '####.']),
    'c': glyph(['.###', '#...', '#...', '#...', '.###'], 2),
    'd': glyph(['....#', '....#', '.####', '#...#', '#...#', '#...#', '.####']),
    'e': glyph(['.###.', '#...#', '#####', '#....', '.###.'], 2),
    'f': glyph(['..##', '.#..', '###.', '.#..', '.#..', '.#..', '.#..']),
    'g': glyph(['.####', '#...#', '#...#', '#...#', '.####', '....#', '.###.'], 2),
    'h': glyph(['#....', '#....', '####.', '#...#', '#...#', '#...#', '#...#']),
    'i': glyph(['#', '.', '#', '#', '#', '#', '#']),
    'j': glyph(['..#', '...', '..#', '..#', '..#', '..#', '..#', '#.#', '.#.']),
    'k': glyph(['#...', '#...', '#..#', '#.#.', '##..', '#.#.', '#..#']),
    'l': glyph(['#.', '#.', '#.', '#.', '#.', '#.', '.#']),
    'm': glyph(['####.', '#.#.#', '#.#.#', '#.#.#', '#.#.#'], 2),
    'n': glyph(['####.', '#...#', '#...#', '#...#', '#...#'], 2),
    'o': glyph(['.###.', '#...#', '#...#', '#...#', '.###.'], 2),
    'p': glyph(['####.', '#...#', '#...#', '#...#', '####.', '#....', '#....'], 2),
    'q': glyph(['.####', '#...#', '#...#', '#...#', '.####', '....#', '....#'], 2),
    'r': glyph(['#.##', '##..', '#...', '#...', '#...'], 2),
    's': glyph(['.####', '#....', '.###.', '....#', '####.'], 2),
    't': glyph(['.#..', '.#..', '###.', '.#..', '.#..', '.#..', '..##']),
    'u': glyph(['#...#', '#...#', '#...#', '#...#', '.####'], 2),
    'v': glyph(['#...#', '#...#', '#...#', '.#.#.', '..#..'], 2),
    'w': glyph(['#...#', '#...#', '#.#.#', '#.#.#', '.#.#.'], 2),
    'x': glyph(['#...#', '.#.#.', '..#..', '.#.#.', '#...#'], 2),
    'y': glyph(['#...#', '#...#', '#...#', '#...#', '.####', '....#', '.###.'], 2),
    'z': glyph(['#####', '...#.', '..#..', '.#...', '#####'], 2),
    '{': glyph(['.##', '.#.', '.#.', '#..', '.#.', '.#.', '.##']),
    '|': glyph(['#', '#', '#', '#', '#', '#', '#']),
    '}': glyph(['##.', '.#.', '.#.', '..#', '.#.', '.#.', '##.']),
    '~': glyph(['.#..#', '#.##.'], 3),
}


def make_font():
    chars = [chr(c) for c in range(32, 127)]
    assert set(FONT) == set(chars), sorted(set(chars) - set(FONT))
    cols = 16
    c = Canvas(cols * FONT_CELL_W, ((len(chars) + cols - 1) // cols) * FONT_CELL_H)
    white = {'#': rgb('#ffffff')}
    for i, ch in enumerate(chars):
        rows = FONT[ch]
        assert len(rows) <= FONT_CELL_H and all(len(r) < FONT_CELL_W for r in rows), repr(ch)
        c.blit(rows, white, (i % cols) * FONT_CELL_W, (i // cols) * FONT_CELL_H)
    c.save('font.png')


# ----------------------------------------------------------------------------------------------
# Backgrounds: assets/gfx/themes/<theme>/sky.png, far.png and near.png, 320x180, repeating
# sideways. Drawn behind the level back to front, each scrolling a bit faster than the last.
# ----------------------------------------------------------------------------------------------

W, H = 320, 180


def periodic_noise(x, seed, octaves=((1, 1.0), (3, 0.5), (7, 0.25), (13, 0.12))):
    """Smooth noise that repeats every 320 px, so the layer tiles seamlessly."""
    v = 0.0
    for freq, amp in octaves:
        v += amp * math.sin(2 * math.pi * freq * x / W + seed * (freq + 1.7))
    return v


def wrapped_dx(x, cx):
    """Distance from x to cx going round the 320 px loop, so shapes near the edge tile too."""
    d = abs(x - cx) % W
    return min(d, W - d)


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(4))


def fill_down(c, color, depth):
    """Fills each column from the top down to depth(x)."""
    for x in range(W):
        for y in range(0, max(0, int(depth(x)))):
            c.put(x, y, color)


def fill_up(c, color, height):
    """Fills each column from the bottom up to height(x)."""
    for x in range(W):
        for y in range(H - max(0, int(height(x))), H):
            c.put(x, y, color)


def disc(c, cx, cy, r, color):
    for y in range(int(cy - r), int(cy + r) + 1):
        for x in range(int(cx - r), int(cx + r) + 1):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                c.put(x % W, y, color)


def sky(top, bottom):
    c = Canvas(W, H)
    a, b = rgb(top), rgb(bottom)
    for y in range(H):
        for x in range(W):
            c.put(x, y, lerp(a, b, y / (H - 1)))
    return c


def stars(c, count, seed, color='#ffffff', below=H):
    rng = random.Random(seed)
    for _ in range(count):
        x, y = rng.randrange(W), rng.randrange(below)
        c.mix(x, y, rgb(color, rng.choice((70, 120, 200))))
        if rng.random() < 0.08:  # a few bright ones with a little cross
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                c.mix(x + dx, y + dy, rgb(color, 70))


def glow(c, cx, cy, r, color, rings=3):
    """A round light with soft rings around it."""
    for i in range(rings, 0, -1):
        rr = r + i * 4
        for y in range(int(cy - rr), int(cy + rr) + 1):
            for x in range(int(cx - rr), int(cx + rr) + 1):
                if (x - cx) ** 2 + (y - cy) ** 2 <= rr * rr:
                    c.mix(x % W, y, rgb(color, 22))
    disc(c, cx, cy, r, rgb(color))


def cloud(c, cx, cy, size, light, shade):
    """A puffy cloud with a flat bottom and a shaded underside."""
    rng = random.Random(cx * 31 + cy)
    puffs = [(cx + dx, cy - rng.uniform(0, size * 0.5), rng.uniform(size * 0.45, size * 0.8))
             for dx in range(-size, size + 1, max(2, size // 2))]
    for px, py, r in puffs:
        for y in range(int(py - r), int(cy) + 1):
            for x in range(int(px - r), int(px + r) + 1):
                if (x - px) ** 2 + (y - py) ** 2 <= r * r:
                    c.put(x % W, y, rgb(shade) if y > cy - 3 else rgb(light))


def cave_layer(color, top_depth, bottom_height, seed, jag):
    """Rocky silhouettes hanging from the top and rising from the bottom."""
    c = Canvas(W, H)
    col = rgb(color)
    for x in range(W):
        top = int(top_depth + 18 * periodic_noise(x, seed) + jag * ((x * 7919 + seed * 13) % 5 - 2))
        bottom = int(bottom_height + 16 * periodic_noise(x, seed + 4.2) + jag * ((x * 104729 + seed) % 5 - 2))
        for y in range(0, max(0, top)):
            c.put(x, y, col)
        for y in range(H - max(0, bottom), H):
            c.put(x, y, col)
    return c


def tree_line(c, color, base, seed, spacing, radius):
    """Round tree tops along a hill whose height is base(x)."""
    rng = random.Random(seed)
    fill_up(c, rgb(color), base)
    for cx in range(0, W, spacing):
        cx += rng.randrange(spacing // 2)
        r = rng.uniform(*radius)
        disc(c, cx, H - base(cx) - r * 0.4, r, rgb(color))


def peaks(seed, count, height, slope):
    """Mountain tops as a height function: the highest of `count` triangles."""
    rng = random.Random(seed)
    tops = [(rng.uniform(0, W), rng.uniform(*height), rng.uniform(*slope)) for _ in range(count)]

    def h(x):
        return max(top - wrapped_dx(x, cx) * k for cx, top, k in tops) + ((x * 7919) % 3 - 1)
    return h


def snowy_peaks(c, rock, snow, h, snowline):
    """Fills the peaks with rock, capped with snow wherever they're taller than `snowline`."""
    for x in range(W):
        height = int(h(x))
        cap = height - snowline + int(3 * periodic_noise(x, 2.0, ((5, 1.0), (11, 0.6))))
        for y in range(H - max(0, height), H):
            c.put(x, y, rgb(snow) if y < H - height + cap else rgb(rock))


def pines(c, color, ground, seed, spacing, height):
    rng = random.Random(seed)
    for cx in range(0, W, spacing):
        cx += rng.randrange(spacing)
        tall = rng.randrange(*height)
        foot = H - int(ground(cx)) + 2
        for i in range(tall):
            half = (i * 0.38) if i < tall - 1 else 0.6
            for x in range(int(cx - half), int(cx + half) + 1):
                c.put(x % W, foot - tall + i, rgb(color))


def chimneys(c, color, window, seed, count):
    """Tall stacks rising from the bottom, with a few lit windows."""
    rng = random.Random(seed)
    for _ in range(count):
        cx, w, tall = rng.randrange(W), rng.randrange(8, 20), rng.randrange(50, 120)
        for y in range(H - tall, H):
            lip = 2 if y < H - tall + 3 else 0
            for x in range(cx - w // 2 - lip, cx + w // 2 + lip):
                c.put(x % W, y, rgb(color))
        for _ in range(rng.randrange(0, 4)):
            wx, wy = cx + rng.randrange(-w // 2 + 2, max(-w // 2 + 3, w // 2 - 2)), H - tall + rng.randrange(8, tall - 6)
            for dy in range(3):
                c.put(wx % W, wy + dy, rgb(window))


def stalactites(c, color, seed, count, length):
    rng = random.Random(seed)
    for _ in range(count):
        cx, w, long = rng.randrange(W), rng.randrange(4, 14), rng.randrange(*length)
        for y in range(long):
            half = w / 2 * (1 - y / long)
            for x in range(int(cx - half), int(cx + half) + 1):
                c.put(x % W, y, rgb(color))


def vines(c, color, leaf, seed, count, length):
    """Strands hanging from the top, with leaves on alternating sides."""
    rng = random.Random(seed)
    for _ in range(count):
        x, long = rng.randrange(W), rng.randrange(*length)
        for y in range(long):
            sway = int(round(math.sin(y / 6 + x) * 1.2))
            c.put((x + sway) % W, y, rgb(color))
            if y % 5 == 2:
                side = 1 if (y // 5) % 2 else -1
                c.put((x + sway + side) % W, y, rgb(leaf))
                c.put((x + sway + 2 * side) % W, y - 1, rgb(leaf))


def sparks(c, seed, count, colors, above):
    """Glowing bits floating up from below."""
    rng = random.Random(seed)
    for _ in range(count):
        x, y = rng.randrange(W), rng.randrange(above, H)
        c.mix(x, y, rgb(rng.choice(colors), rng.choice((90, 160, 230))))


# ----------------------------------------------------------------------------------------------
# Themes. Each one is a tiles.png plus three background layers in assets/gfx/themes/<name>/, and
# assets/levels/levels.txt says which levels use which. A theme changes the colours of the rock
# and what grows on it; spikes, hook blocks and platforms look the same everywhere, so they're
# always easy to spot (except that on snow the spikes are dark iron, as white ones would vanish).
# ----------------------------------------------------------------------------------------------

def make_cave():
    make_tiles('cave', tile_sheet(ROCK))
    sky('#22263a', '#10121a').save('themes/cave/sky.png')
    cave_layer('#232638', top_depth=38, bottom_height=44, seed=1.3, jag=1).save('themes/cave/far.png')
    cave_layer('#1a1c2a', top_depth=22, bottom_height=28, seed=5.1, jag=2).save('themes/cave/near.png')


# Garden: brown earth under thick grass, ferns, red flowers and vines hanging from ceilings.
GARDEN = {'0': rgb('#24170f'), '1': rgb('#43301f'), '2': rgb('#5a412a'), '3': rgb('#76583a'),
          'g': rgb('#2b6234'), 'G': rgb('#3f9446'), 'h': rgb('#78cc54'), 'H': rgb('#bdf07c'),
          'f': rgb('#ffe27a'), 'p': rgb('#f0604a')}
garden_grass = ['HhHHhHHh',
                'hGhhGhhG',
                'GgGGgGgG',
                'gGgg1gGg',
                '.g.G.g.g',
                '.G...G..',
                '.g......',
                '........']
garden_grass_b = ['hHhHHhHH',
                  'GhGhhGhG',
                  'gGgGGgGg',
                  'Gg1gGg1g',
                  'g.G.g.G.',
                  '..g...g.',
                  '......G.',
                  '........']
fern = ['........',
        '........',
        '...h....',
        '.h.hG.h.',
        '.Gh.hGh.',
        'h.GhG.G.',
        '.GhGhG..',
        'GhGgGhG.']
poppies = ['........',
           '..p.....',
           '.pfp..p.',
           '..p..pfp',
           '..G...p.',
           '..G..G..',
           '.hG.Gh..',
           '.GhGhG..']
vine = ['.g..Gg..',
        '.G...G..',
        '.Gh..g..',
        '..G..Gh.',
        '.hG...G.',
        '..g...g.',
        '..h.....',
        '........']


def make_garden():
    make_tiles('garden', tile_sheet(GARDEN, (garden_grass, garden_grass_b), (fern, poppies, vine)))
    c = sky('#1b2b3d', '#3a5a45')
    sparks(c, seed=3, count=40, colors=('#e8f08c', '#b8f070'), above=90)  # fireflies
    c.save('themes/garden/sky.png')
    c = Canvas(W, H)
    tree_line(c, '#22402f', lambda x: 42 + 12 * periodic_noise(x, 0.7), seed=4, spacing=16, radius=(9, 15))
    c.save('themes/garden/far.png')
    c = Canvas(W, H)
    vines(c, '#152a1f', '#1b3627', seed=5, count=26, length=(12, 50))
    fill_down(c, rgb('#152a1f'), lambda x: 6 + 4 * periodic_noise(x, 1.9))
    tree_line(c, '#152a1f', lambda x: 16 + 8 * periodic_noise(x, 3.3), seed=6, spacing=22, radius=(8, 13))
    c.save('themes/garden/near.png')


# Summit: grey-blue stone under snow, icicles, little pines, and mountains under a moonlit sky.
SNOW = {'0': rgb('#1c2232'), '1': rgb('#384258'), '2': rgb('#4c5870'), '3': rgb('#6a7892'),
        'g': rgb('#8ea4c2'), 'G': rgb('#c2d4ea'), 'h': rgb('#e6f0fa'), 'H': rgb('#ffffff'),
        't': rgb('#2c5a48'), 'T': rgb('#3f7a5a'), 'w': rgb('#5a4030')}
snow_top = ['HHHHHHHH',
            'HhHHHHhH',
            'hhhHhhhh',
            'GhG1GhGG',
            '.G...G..',
            '........',
            '........',
            '........']
snow_top_b = ['HHHHHHHH',
              'HHHhHHHh',
              'hhhhhHhh',
              'GGhGG1hG',
              '..G...G.',
              '........',
              '........',
              '........']
snow_lump = ['........',
             '........',
             '........',
             '........',
             '........',
             '........',
             '..hHH...',
             '.hHHHHh.']
pine = ['...T....',
        '..tTt...',
        '..TtH...',
        '.tTtTt..',
        '.HtTtT..',
        'tTtTtHt.',
        '...w....',
        '...w....']
IRON = {'1': rgb('#a4acc0'), '2': rgb('#5c6478'), '3': rgb('#262a38')}
icicles = ['hHHh.hHh',
           '.hH...h.',
           '.hH...h.',
           '..h.....',
           '..h.....',
           '........',
           '........',
           '........']


def make_snow():
    make_tiles('snow', tile_sheet(SNOW, (snow_top, snow_top_b), (snow_lump, pine, icicles), IRON))
    c = sky('#121a32', '#3a4c74')
    stars(c, count=110, seed=7, below=120)
    glow(c, 246, 34, 9, '#eef2fa')
    for x, y in ((243, 31), (249, 37), (245, 38)):  # craters
        c.put(x, y, rgb('#c8d0e0'))
    c.save('themes/snow/sky.png')
    c = Canvas(W, H)
    snowy_peaks(c, '#28344f', '#56688c', peaks(8, 7, (70, 120), (0.9, 1.6)), snowline=78)
    c.save('themes/snow/far.png')
    c = Canvas(W, H)
    ground = lambda x: 26 + 8 * periodic_noise(x, 2.6)
    fill_up(c, rgb('#18203a'), ground)
    pines(c, '#18203a', ground, seed=9, spacing=14, height=(14, 30))
    c.save('themes/snow/near.png')


# Chimney: black basalt with glowing cracks and embers, and a furnace glow from below.
EMBER = {'0': rgb('#150d10'), '1': rgb('#2a1c20'), '2': rgb('#3d282a'), '3': rgb('#583834'),
         'g': rgb('#3a1c1a'), 'G': rgb('#5c2620'), 'h': rgb('#e8662a'), 'H': rgb('#ffc04a'),
         'w': rgb('#d8c0a0')}
ember_top = ['GGgGGGgG',
             'gHgghgGg',
             'h1gG1hg1',
             '.h..1.h.',
             '....h...',
             '........',
             '........',
             '........']
ember_top_b = ['GgGGGgGG',
               'gGhgGgHg',
               '1gh1gGh1',
               '..h..1h.',
               '..h.....',
               '........',
               '........',
               '........']
coals = ['........',
         '........',
         '........',
         '........',
         '........',
         '....H...',
         '..h.hH..',
         '.GhGhGh.']
glowshroom = ['........',
              '........',
              '........',
              '...hHh..',
              '..hHHHh.',
              '....w...',
              '....w...',
              '...GwG..']
lava_drip = ['00h1h00.',
             '.0hH0...',
             '..hH....',
             '..h.....',
             '........',
             '..H.....',
             '........',
             '........']


def make_ember():
    make_tiles('ember', tile_sheet(EMBER, (ember_top, ember_top_b), (coals, glowshroom, lava_drip)))
    c = sky('#170a0e', '#5c2016')
    sparks(c, seed=11, count=70, colors=('#ffb040', '#ff7a30', '#ffe08a'), above=40)
    c.save('themes/ember/sky.png')
    c = Canvas(W, H)
    chimneys(c, '#2e1517', '#e8762c', seed=12, count=13)
    c.save('themes/ember/far.png')
    c = Canvas(W, H)
    stalactites(c, '#1c0c0f', seed=13, count=30, length=(10, 46))
    fill_down(c, rgb('#1c0c0f'), lambda x: 8 + 5 * periodic_noise(x, 4.4))
    fill_up(c, rgb('#1c0c0f'), lambda x: 20 + 10 * periodic_noise(x, 6.1))
    c.save('themes/ember/near.png')


# Daylight: warm sandstone, bright grass and flowers, a blue sky with clouds and green hills.
SUN = {'0': rgb('#4a3020'), '1': rgb('#8c6440'), '2': rgb('#a87a50'), '3': rgb('#c8986a'),
       'g': rgb('#3a8436'), 'G': rgb('#56b446'), 'h': rgb('#98dc5c'), 'H': rgb('#d6f68e'),
       'f': rgb('#fff27a'), 'p': rgb('#ff6aa8')}
daisies = ['........',
           '........',
           '.....f..',
           '....fpf.',
           '.f...f..',
           'fpf..G..',
           '.f..hG..',
           '.G.GhG..']
roots = ['.0..0.0.',
         '.0..0.0.',
         '..0.0...',
         '..0..0..',
         '.0...0..',
         '.....0..',
         '........',
         '........']


def make_daylight():
    make_tiles('daylight', tile_sheet(SUN, decor=(tuft, daisies, roots)))
    c = sky('#3c78bc', '#a4cfe8')
    glow(c, 58, 30, 11, '#fff4c8', rings=4)
    for x, y, size in ((150, 42, 14), (232, 26, 10), (300, 60, 12), (100, 74, 8), (20, 96, 9)):
        cloud(c, x, y, size, '#f6faff', '#d4e2f2')
    c.save('themes/daylight/sky.png')
    c = Canvas(W, H)
    fill_up(c, rgb('#7eaec6'), lambda x: 62 + 22 * periodic_noise(x, 0.4))
    c.save('themes/daylight/far.png')
    c = Canvas(W, H)
    tree_line(c, '#4a8458', lambda x: 30 + 12 * periodic_noise(x, 5.5), seed=14, spacing=34, radius=(6, 11))
    c.save('themes/daylight/near.png')


# Ruins: old stone bricks under glowing moss, pink glowing mushrooms, hanging roots tipped with
# light, and broken arches in the dark behind.
RUINS = {'0': rgb('#1a1528'), '1': rgb('#3a3050'), '2': rgb('#4e4468'), '3': rgb('#6a5c88'),
         'g': rgb('#2a5a5a'), 'G': rgb('#3a8a7a'), 'h': rgb('#5ae0c0'), 'H': rgb('#b0fff0'),
         'p': rgb('#ff7ad8'), 'P': rgb('#b8409a'), 'f': rgb('#ffe0f8'), 'r': rgb('#5a4430')}
bricks_a = ['33303333',
            '22202222',
            '21201112',
            '00000000',
            '33333033',
            '22222022',
            '11211021',
            '00000000']
bricks_b = ['33333303',
            '22222202',
            '12112101',
            '00000000',
            '33033333',
            '22022222',
            '21021112',
            '00000000']
bricks_c = ['33303333',   # cracked
            '22202022',
            '21201102',
            '00000000',
            '33333033',
            '22202022',
            '11011021',
            '00000000']
bricks_deep = [r.replace('1', '0').replace('2', '1').replace('3', '1') for r in bricks_a]
glow_moss = ['HhHHhHHh',
             'hGhhGhhG',
             'GgGGgGgG',
             'g2gg2g2g',
             '.g...g..',
             '........',
             '........',
             '........']
glow_moss_b = ['hHhHHhHH',
               'GhGhhGhG',
               'gGgGGgGg',
               '2g2gg2g2',
               '..g...g.',
               '........',
               '........',
               '........']
sprouts = ['........',
           '........',
           '........',
           '........',
           '..h.....',
           '..G..h..',
           '.hG.hG.h',
           '.GG.GG.G']
glowcap = ['........',
           '........',
           '..pppp..',
           '.pfppfp.',
           'PPPPPPPP',
           '...ff...',
           '...ff...',
           '..rffr..']
root = ['r..r.rr.',
        'r..r..r.',
        '.r.r..r.',
        '.r..r.r.',
        '.h..r..h',
        '....h...',
        '........',
        '........']


def arches(c, color, seed, count):
    """Broken arches and pillars standing on the ground."""
    rng = random.Random(seed)
    col = rgb(color)
    for _ in range(count):
        cx, w, tall = rng.randrange(W), rng.randrange(18, 34), rng.randrange(60, 110)
        for y in range(H - tall, H):
            for x in (cx - w // 2, cx - w // 2 + 1, cx - w // 2 + 2, cx + w // 2 - 2, cx + w // 2 - 1, cx + w // 2):
                c.put(x % W, y, col)
        # The arch over the pillars, sometimes broken off.
        r = w // 2
        broken = rng.random() < 0.4
        for a in range(0, 181, 2):
            if broken and a > 120:
                break
            for thick in range(3):
                x = int(cx + (r - thick) * math.cos(math.radians(a)))
                y = int(H - tall - (r - thick) * math.sin(math.radians(a)))
                c.put(x % W, y, col)


def make_ruins():
    make_tiles('ruins', tile_sheet(RUINS, (glow_moss, glow_moss_b), (sprouts, glowcap, root),
                                   rocks=(bricks_a, bricks_b, bricks_c, bricks_deep)))
    c = sky('#160f24', '#14303a')
    sparks(c, seed=21, count=60, colors=('#5ae0c0', '#b0fff0', '#ff7ad8'), above=20)  # spores
    c.save('themes/ruins/sky.png')
    c = Canvas(W, H)
    arches(c, '#241c3a', seed=22, count=7)
    fill_up(c, rgb('#241c3a'), lambda x: 30 + 8 * periodic_noise(x, 2.2))
    c.save('themes/ruins/far.png')
    c = Canvas(W, H)
    vines(c, '#120c1e', '#2a6a5a', seed=23, count=30, length=(10, 60))
    fill_down(c, rgb('#120c1e'), lambda x: 5 + 4 * periodic_noise(x, 3.1))
    # Giant mushrooms on the ground, close.
    rng = random.Random(24)
    for _ in range(6):
        cx, r, stem = rng.randrange(W), rng.randrange(8, 16), rng.randrange(14, 30)
        for y in range(H - stem, H):
            for x in range(cx - 2, cx + 3):
                c.put(x % W, y, rgb('#120c1e'))
        for y in range(-r, 1):
            for x in range(-r, r + 1):
                if x * x + y * y <= r * r:
                    c.put((cx + x) % W, H - stem + y, rgb('#120c1e'))
    fill_up(c, rgb('#120c1e'), lambda x: 14 + 6 * periodic_noise(x, 4.7))
    c.save('themes/ruins/near.png')


# ----------------------------------------------------------------------------------------------
# Props: assets/gfx/props.png, the scenery in rooms (a Prop entity's `kind`), and
# assets/gfx/props.txt, saying where each is in the sheet and how it's drawn, for the game:
#   name x y width height frames fps hangs light_r light_g light_b light_radius
# Standing props sit on the bottom of their tile, hanging ones hang from its top. Animated ones
# have their frames side by side. Every prop gets a dark outline.
# ----------------------------------------------------------------------------------------------

PROP = {k: rgb(v) for k, v in {
    'K': '#1a1614', 'k': '#2e2622', 'a': '#6a6460', 's': '#8a8a94', 'S': '#5a5a66',
    'w': '#8a5a34', 'W': '#5a3a20', 'l': '#b47c48',
    'g': '#4ac08a', 'G': '#2a8a5a', 'c': '#c8b890', 'C': '#9a8a64',
    'r': '#c8443a', 'R': '#8a2a2a', 'o': '#ff9a3a', 'y': '#ffe07a', 'f': '#fff4d0',
    'b': '#6ae0ff', 'B': '#2a8fb0', 'i': '#cfefff', 'I': '#8ab8d8',
    'p': '#e87bd0', 'P': '#a04a90', 'm': '#e8dcc8',
    'u': '#6a6080', 'U': '#4a4060', 't': '#5fd0b0', 'T': '#2a8a78',
    'e': '#a8483a', 'n': '#3a5a9a', 'h': '#4a8a4a', 'x': '#e8dcc0',
    'z': '#9aa6bc', 'Z': '#5a6478', 'q': '#2f6a4a', 'Q': '#1f4a34', 'v': '#f0f4ff',
    'j': '#5a9a3a', 'J': '#3a6a2a', 'L': '#8ac850', 'd': '#e8c04a', 'D': '#4a7ac8',
    '1': '#7af0e0', '2': '#2aa8a0', '3': '#c8a0ff', '4': '#7a50c8', '6': '#e8fff8',
    '7': '#2f6a40', '8': '#4f9a5a',
}.items()}
PROP['#'] = OUTLINE


def flame_frames(base, flames):
    """Frames of a fire: the same `base` under each set of `flames` rows."""
    return [f + base for f in flames]


FIRE_BASE = ['...WwlwW....',
             '..WwWlWwW...',
             '.sSWwwwWsS..',
             'sSsSsSsSsSs.']
SIGNAL_BASE = ['.....WwlwWlwW.......',
               '....WwWlWwWlWW......',
               '...WwlWwWlwWwlW.....',
               '.sSsWwwWwwWwwWsSs...',
               'sSsSsSsSsSsSsSsSsS..',
               'SsSsSsSsSsSsSsSsSsS.']

def lantern_string(width=36, height=16):
    """Two poles and a cord sagging between them, little lanterns hanging off it."""
    g = [['.'] * width for _ in range(height)]
    for y in range(height):
        g[y][0] = g[y][width - 1] = 'W'
    g[height - 1][1] = g[height - 1][width - 2] = 'W'
    mid = (width - 1) / 2
    for x in range(1, width - 1):
        sag = 1 + round(4 * (1 - ((x - mid) / mid) ** 2))
        g[sag][x] = 'k'
        if x % 6 == 3:
            g[sag + 1][x], g[sag + 2][x] = 'y', 'f'
            g[sag + 2][x - 1] = g[sag + 2][x + 1] = 'y'
            g[sag + 3][x] = 'o'
    return [''.join(row) for row in g]


PROPS = [
    # name, frames, fps, hangs, light (r, g, b, radius)
    ('cold_fire', [['....KK..K.....',
                    '...kKkakKk....',
                    '..kaakaakak...',
                    '.sSsaaaaaasSs.',
                    'sSsSsSsSsSsSsS']], 0, False, None),
    ('pack', [['..CCCCCC..',
               '.CccccccC.',
               '.CGGGGGGC.',
               '.CcgccgcC.',
               '.CcgccgcC.',
               'CCccccccCC',
               'CcccCCcccC',
               'CccCwwCccC',
               'CccCwwCccC',
               'CcccCCcccC',
               '.CCCCCCCC.']], 0, False, None),
    ('bedroll', [['...........RRR..',
                  'rrrrrrrrrrRrrrR.',
                  'rdrrrrdrrrRrRrR.',
                  'RRRRRRRRRRRrrrR.',
                  '...........RRR..']], 0, False, None),
    ('fire', flame_frames(FIRE_BASE, [
        ['.....y......', '....yo......', '....oyo..y..', '...oyfyo.o..', '..oyffyoo...', '..oyfffyo...', '...ofyfo....'],
        ['......y.....', '.y...oy.....', '.o..oyo.....', '...oyfyo....', '..ooyfyoo...', '..oyfffyo...', '...ofyfo....'],
        ['....y.......', '....oy...y..', '...oyo...o..', '...oyfyoo...', '..oyffyyo...', '..oyfffyo...', '...ofyfo....'],
    ]), 8, False, (255, 160, 80, 52)),
    ('tent', [['..............W...............',
               '.............CcC..............',
               '............CcccC.............',
               '...........CcccccC............',
               '..........CcccKcccC...........',
               '.........CcccKKKcccC..........',
               '........CcccKKKKKcccC.........',
               '.......CcccKKKKKKKcccC........',
               '......CcccKKKKKKKKKcccC.......',
               '.....CcccKKKKKKKKKKKcccC......',
               '....CcccKKKKKKKKKKKKKcccC.....',
               '...CcccKKKKKKKKKKKKKKKcccC....',
               '..CcccKKKKKKKKKKKKKKKKKcccC...',
               '.CCCCCKKKKKKKKKKKKKKKKKCCCCC..']], 0, False, None),
    ('lantern_post', [['..ZZZ..', '.ZyyyZ.', '.ZyfyZ.', '.ZyyyZ.', '..ZZZ..'] + ['...W...'] * 15 + ['..WWW..']],
     0, False, (255, 190, 110, 40)),
    ('hanging_lantern', [['...Z...', '...z...', '...Z...', '...z...', '...Z...', '...z...',
                          '..ZZZ..', '.ZyyyZ.', '.ZyfyZ.', '.ZyyyZ.', '..ZZZ..', '...Z...']],
     0, True, (255, 190, 110, 36)),
    ('stalagmite', [['...s...', '...s...', '..sS...', '..ss...', '..sSs..', '.ssSs..',
                     '.sssS..', '.sSssS.', 'ssSsssS', 'sSssSsS']], 0, False, None),
    ('stalactites', [['sSssSsSsSsssSs',
                      '.sSs.sSs.SsS..',
                      '.sS..sS...sS..',
                      '..s...S...s...',
                      '..s.......s...',
                      '..........s...']], 0, True, None),
    ('bones', [['..xxx.........',
                '.xKxKx..xx.xx.',
                '.xxxxx...xxx..',
                '..x.x...xx.xx.']], 0, False, None),
    ('crystals', [['.....b......',
                   '....bbB.....',
                   '....bbB..b..',
                   '.b..bbB.bbB.',
                   'bbB.bbB.bbB.',
                   'bbB.bbBbbbB.',
                   'bbBbbbBbbB..',
                   'BbBbBbBbB...',
                   'SsSsSsSsSs..']], 0, False, (90, 200, 255, 30)),
    ('mushrooms', [['..ppp.......',
                    '.pPpPp...pp.',
                    'pPpppPp.pPPp',
                    '...m....pPPp',
                    '...m.pp..m..',
                    '...mpPPp.m..',
                    '...m.m...m..',
                    '..mm.m..mm..']], 0, False, (230, 110, 210, 26)),
    ('broken_pillar', [['...uU.....',
                        '..uuuUu...',
                        '.uuUuuuUu.',
                        'UuuuUuuuuU',
                        '.uUuUuUuU.'] + ['.uUuUuUuU.', '.uUtUuUuU.', '.uUuUuTuU.', '.uUuUuUuU.'] * 4
                       + ['UUUUUUUUUU', 'uuuuuuuuuu']], 0, False, None),
    ('banner', [['zzzzzzzzzz',
                 '.tttttttt.',
                 '.tTttttTt.',
                 '.tTtddtTt.',
                 '.tTddddTt.',
                 '.tTtddtTt.'] + ['.tTttttTt.'] * 10 + ['.tt.tt.tt.', '.t...t..t.']], 0, True, None),
    ('bookshelf', [['WWWWWWWWWWWWWWWWWWWWWW',
                    'WwwwwwwwwwwwwwwwwwwwwW',
                    'WennhexehnneehxnhenneW',
                    'WennhexehnneehxnhenneW',
                    'WennhexehnneehxnhenneW',
                    'WwwwwwwwwwwwwwwwwwwwwW',
                    'WhnexeenhhenxnehenheeW',
                    'WhnexeenhhenxnehenheeW',
                    'WhnexeenhhenxnehenheeW',
                    'WwwwwwwwwwwwwwwwwwwwwW',
                    'WnehhxennehxxnnehenxhW',
                    'WnehhxennehxxnnehenxhW',
                    'WnehhxennehxxnnehenxhW',
                    'WwwwwwwwwwwwwwwwwwwwwW',
                    'WxxneehnhexnnehhenxxeW',
                    'WxxneehnhexnnehhenxxeW',
                    'WxxneehnhexnnehhenxxeW',
                    'WWWWWWWWWWWWWWWWWWWWWW']], 0, False, None),
    ('candles', [['.y...y....',
                  '.o...o..y.',
                  '.x...x..o.',
                  '.x.y.x..x.',
                  '.x.o.x..x.',
                  '.x.x.x..x.',
                  'zzzzzzzzzz']], 0, False, (255, 200, 120, 30)),
    ('statue', [['....uuu.....',
                 '...uUuuU....',
                 '...uKKKu....',
                 '...uKtKu....',
                 '...uUuuU....',
                 '..uuUuuUu...',
                 '.uuUuuuuUu..',
                 '.uUuuuuuuUu.',
                 '.uUuuUuuuUu.',
                 '.uUuuUuuuUu.',
                 '.uUuuUuuuUu.',
                 '.uUuuUuuuUu.',
                 '.uUuuUuuuUu.',
                 '.uUuuUuuuUu.',
                 '..uUuUuuUu..',
                 '..uUuUuuUu..',
                 '.UUUUUUUUUU.',
                 'uuuuuuuuuuuu',
                 'UUUUUUUUUUUU']], 0, False, None),
    ('signpost', [['..llllllll..',
                   '.lwwwwwwwwl.',
                   'lwwwwwwwwwl.',
                   '.llllWlllll.',
                   '.....W......',
                   '..llllllll..',
                   '..lwwwwwwwwl',
                   '..lwwwwwwwwwl'[:12],
                   '..lllllWlll.',
                   '.....W......',
                   '.....W......',
                   '.....W......',
                   '.....W......',
                   '....WWW.....']], 0, False, None),
    ('map_table', [['..xxxxxxxxxxxxxx..',
                    '.xxdxxDxxxxhxxxxx.',
                    '.xhxxxxxxDxxxdxxx.',
                    'WWWWWWWWWWWWWWWWWW',
                    'wwwwwwwwwwwwwwwwww',
                    '.W..............W.',
                    '.W..............W.',
                    '.W..............W.',
                    '.W..............W.']], 0, False, None),
    ('flowers', [['.d....r.....',
                  'dyd..rdr..p.',
                  '.d.L..r..pyp',
                  '.L.L..L...p.',
                  'jLjLjjLjjjLj']], 0, False, None),
    ('vine', [['jJj....', '.jJ....', '.jJL...', '..jJ...', '..jJ...', '.LjJ...', '..jJ...',
               '...jJ..', '...jJL.', '..jJ...', '.LjJ...', '..jJ...', '..jJ...', '...jJ..',
               '...jJL.', '..jJ...', '..j....', '..L....']], 0, True, None),
    ('pine', [['.......v.......',
               '......vqv......',
               '.....vqqqv.....',
               '....vqqQqqv....',
               '......qQq......',
               '....vqqQqqv....',
               '...vqqqQqqqv...',
               '..vqqqqQqqqqv..',
               '.....qqQqq.....',
               '...vqqqQqqqv...',
               '..vqqqqQqqqqv..',
               '.vqqqqqQqqqqqv.',
               'vqqqqqqQqqqqqqv',
               '....qqqQqqq....',
               '...qqqqQqqqq...',
               '..qqqqqQqqqqq..',
               '.qqqqqqQqqqqqq.',
               'QQQQQQQwQQQQQQQ',
               '.......w.......',
               '.......w.......',
               '......WwW......']], 0, False, None),
    ('flags', [['W............................W',
                'Wrr.dd..DD..gg..rr..dd..DD..gW',
                'WRr.dD..DD..gG..Rr..dD..DD..gW',
                'W.r..d...D...g...r...d...D...W',
                'W............................W',
                'W............................W',
                'W............................W',
                'W............................W',
                'W............................W',
                'WW..........................WW']], 0, False, None),
    ('brazier', flame_frames(['.zZzZzZzZ.', '..zZzZzZ..', '...Z..Z...', '..Z....Z..', '.Z......Z.'], [
        ['....y.....', '...oy.....', '..oyfo.y..', '..yffyoo..'],
        ['.....y....', '..y.yo....', '..oofyo...', '..oyffyo..'],
        ['...y......', '...yo..y..', '..oyfo.o..', '..oyffyo..'],
    ]), 9, False, (255, 140, 60, 44)),
    ('chains', [['.Z.', 'z.z', '.Z.', 'z.z', '.Z.', 'z.z', '.Z.', 'z.z', '.Z.', 'z.z', '.Z.', 'z.z',
                 '.Z.', 'z.z', '.Z.', 'z.z', '.Z.', 'z.z', '.Z.', 'z.z', '.Z.', 'ZZZ']], 0, True, None),
    ('signal_fire', flame_frames(SIGNAL_BASE, [
        ['.........y..........', '........yo..........', '.......oyo...y......', '......oyfyo..o......',
         '.....oyffyoyo.......', '....oyfffffyo.......', '....oyfffffyoo......', '...ooyfffffyo.......',
         '....oyfyfyfo........'],
        ['..........y.........', '...y.....oy.........', '...o....oyo.........', '.......oyfyo........',
         '......oyffyoo.......', '....ooyfffffyo......', '...oyyfffffyo.......', '...ooyfffffyoo......',
         '....oyfyfyfo........'],
        ['........y...........', '........oy....y.....', '.......oyo....o.....', '......oyfyoo........',
         '.....oyffyyo........', '....oyfffffyo.......', '...ooyfffffyo.......', '....oyfffffyoo......',
         '....oyfyfyfo........'],
    ]), 7, False, (255, 170, 90, 80)),
    ('cairn', [['...sSs....',
                '..sSsSs...',
                '...sSs....',
                '..sSssSs..',
                '.sSsSsSsS.',
                '..sSsSsS..',
                '.sSsSsSsSs',
                'sSsSsSsSsS']], 0, False, None),
    ('vent', [['.ZzZzZzZzZzZzZ.',
               'ZzKzKzKzKzKzKzZ',
               'ZZZZZZZZZZZZZZZ']], 0, False, (255, 140, 60, 20)),
    ('ring', [['...zzz...',
               '.zzZZZzz.',
               '.zZ...Zz.',
               'zZ.....Zz',
               'zZ.....ZZ',
               'zZ.....ZZ',
               '.ZZ...ZZ.',
               '.ZZZZZZZ.',
               '...ZZZ...']], 0, False, None),
    ('lamp', [['..Z..', '.ZZZ.', 'ZsSsZ', 'ZSsSZ', 'ZsSsZ', '.ZZZ.']], 0, True, None),
    ('lamp_lit', [['..Z..', '.ZZZ.', 'ZyfyZ', 'ZfyfZ', 'ZyfyZ', '.ZZZ.']], 0, True, (255, 200, 110, 60)),
    ('icicle', [['iIi', 'iIi', 'iI.', '.I.', '.I.', '.i.']], 0, True, None),
    # For the camp: firewood stacked by the tent, a stewpot on its embers, and a string of lanterns
    # between two poles.
    ('woodpile', [['....wlw.wlw....',
                   '...WwlwWwlwW...',
                   '..wlwWwlwWwlw..',
                   '.WwlwWwlwWwlwW.',
                   'wlwWwlwWwlwWwlw',
                   'WwlwWwlwWwlwWwl']], 0, False, None),
    ('stewpot', [['...m...m...',
                  '....m.m....',
                  '..KkkkkkK..',
                  '.KaSSSSSaK.',
                  '.KaSsSsSaK.',
                  '..KaaaaaK..',
                  '.oryoyroyo.',
                  'sSoSrSoSrSs'],
                 ['....m...m..',
                  '...m...m...',
                  '..KkkkkkK..',
                  '.KaSSSSSaK.',
                  '.KaSsSsSaK.',
                  '..KaaaaaK..',
                  '.royoryoyr.',
                  'sSrSoSrSoSs']], 3, False, (255, 150, 70, 22)),
    ('lantern_string', [lantern_string()], 0, False, (255, 210, 130, 40)),
    # A stone block, two tiles square, with an iron ring in it for the rope; and the weight plate a
    # stone holds down, up and down.
    ('stone', [['sssssssssssssS',
                'saaaaaaaaaaaaS',
                'saSaaaaaaaaaaS',
                'saaaaaakkaaaaS',
                'saaaaaZzzZaaaS',
                'saaaaZaaaaZaaS',
                'saaaaZaaaaZaaS',
                'saaaaaZZZZaaaS',
                'saaaaaaaaaaaaS',
                'saaaaaaaaaaSaS',
                'saaaaaaaaaaaaS',
                'saaSaaaaaaaaaS',
                'saaaaaaaaaaaaS',
                'SSSSSSSSSSSSSS']], 0, False, None),
    ('plate', [['.zzzzzzzzzzzz.',
                'zZZZZZZZZZZZZz',
                'ZZZZZZZZZZZZZZ']], 0, False, None),
    ('plate_down', [['1222222222221']], 0, False, (90, 230, 200, 26)),
    ('stall', [['rrrrrrrrrrrrrrrrrrrrrr',
                'rffrffrffrffrffrffrffr',
                'rrrrrrrrrrrrrrrrrrrrrr',
                '.W..................W.',
                '.W..................W.',
                '.W..dd...gg...DD....W.',
                '.W.dyyd.gGGg.DDDD...W.',
                'WWWWWWWWWWWWWWWWWWWWWW',
                'wwwwwwwwwwwwwwwwwwwwww',
                '.W..................W.',
                '.W..................W.',
                '.W..................W.']], 0, False, None),
    ('anvil', [['ZZZZZZZZZZZ.',
                '.ZzzzzzzzZZZ',
                '...ZzzzZ....',
                '...ZzzzZ....',
                '..ZZZZZZZ...',
                '.ZZZZZZZZZ..']], 0, False, None),
    ('fishing_hole', [['.........W.....',
                       '........W.z....',
                       '.......W...z...',
                       '......W.....z..',
                       '.............z.',
                       'IiiIiiiiiiIiIi.',
                       'iBBBBBBBBBBBBi.',
                       'IiiiiIiiiiiiiI.']], 0, False, None),
    ('icicles', [['iiIiiIiiIiiIii',
                  '.iI.iI..iI.iI.',
                  '.iI.i...iI..I.',
                  '..I.i....I..I.',
                  '..I......I....',
                  '.........I....']], 0, True, None),
    # The Hidden Lake and the Glade: magic, glowing.
    ('lily', [['...3..3...',
               '..3663663.',
               '...36663..',
               '.778777877',
               '7788787887']], 0, False, (200, 160, 255, 18)),
    ('glowshroom', [['...1111.....',
                     '..112211....',
                     '.11222211...',
                     '.22222222...',
                     '....66...11.',
                     '....66..1221',
                     '....66..2222',
                     '...666...66.',
                     '...66....66.',
                     '..6666..666.']], 0, False, (122, 240, 224, 30)),
    ('bellflowers', [['.3.....3.',
                      '343...343',
                      '.3..3..3.',
                      '.8.343.8.',
                      '.8..3..8.',
                      '..8.8.8..',
                      '..88888..',
                      '...878...']], 0, False, (200, 160, 255, 20)),
    ('runestone', [['...uuuu...',
                    '..uuuuuu..',
                    '.uuUuuuuu.',
                    '.uu1uuUuu.',
                    '.uu11uuuu.',
                    '.uuu1uuuu.',
                    '.uuUuu1uu.',
                    '.uuuu11uu.',
                    '.uuuuu1uU.',
                    '.uU1uuuuu.',
                    '.uu11uuuu.',
                    '.uuu1uuUu.',
                    '.uuuuuuuu.',
                    'uuUuuuuuuu',
                    'UUUUUUUUUU']], 0, False, (122, 240, 224, 26)),
    ('glow_roots', [['WWwWWwWWwWWw',
                     '.W..w.W..w..',
                     '.W..w..W.w..',
                     '.w..W..W..w.',
                     '..w.W..w..w.',
                     '..1.w...w.1.',
                     '.....w..1...',
                     '.....1......']], 0, True, (122, 240, 224, 22)),
    ('violet_crystals', [['.....3......',
                          '....343.....',
                          '....343..3..',
                          '.3..343.343.',
                          '343.343.343.',
                          '343.3443343.',
                          '3433443.343.',
                          '34434443443.',
                          '444444444444']], 0, False, (200, 160, 255, 32)),
    # Odo's road works.
    ('wheelbarrow', [['.wwwwwwwwww.....',
                      'wWlllllllllw....',
                      '.wWllllllllw....',
                      '..wWWWWWWWw.....',
                      '...W...W...WWWWW',
                      '..ZZZ..W........',
                      '.ZzzzZ.W........',
                      '.ZzKzZ.W........',
                      '..ZZZ..W........']], 0, False, None),
    ('rubble', [['..........zZ....',
                 '.........zZW....',
                 '..........W.....',
                 '.........W......',
                 '........W.......',
                 '......aaW.......',
                 '....aasaWaa.....',
                 '...asSaasaaS....',
                 '..aSaasSaaSaa...',
                 '.aasaSaasaasSa..']], 0, False, None),
    ('planks', [['..llllllllllll..',
                 '..WwwwwwwwwwwW..',
                 'llllllllllllll..',
                 'WwwwwwwwwwwwwW..',
                 '.lllllllllllllll',
                 '.WwwwwwwwwwwwwwW']], 0, False, None),
    ('road_sign', [['rfrfrfrfrfrfrf',
                    'frfrfrfrfrfrfr',
                    'rfrfrfrfrfrfrf',
                    '.W..........W.',
                    '.W..........W.',
                    '.W..........W.',
                    '.W..........W.',
                    '.W..........W.',
                    'WWW........WWW']], 0, False, None),
    # The camp.
    ('log', [['..WWWWWWWWWWWW..',
              '.WwwlwwwwwlwwwCC',
              'WwwwwwwlwwwwwCcC',
              'WwlwwwwwwwwlwCCc',
              '.WWWWWWWWWWWWWCC']], 0, False, None),
    # The old line: a mine cart (asleep, chained, until the beacons wake it; rolling, with you in
    # it), the buffer at the end of the line, and the timbers holding the tunnels up.
    ('cart', [['ZzzzzzzzzzzZ',
               'ZwWwwwwWwwwZ',
               'ZwwwWwwwwWwZ',
               'ZWwwwwWwwwwZ',
               'ZZZZZZZZZZZZ',
               '.ZKZ....ZKZ.',
               '..Z......Z..']], 0, False, None),
    ('cart_off', [['ZzzzzzzzzzzZ',
                   'ZwWzwwwWzwwZ',
                   'ZwwwzwwzwWwZ',
                   'ZWwwwddwwwwZ',
                   'ZZZZZddZZZZZ',
                   '.ZKZ.zz.ZKZ.',
                   '..Z..zz..Z..']], 0, False, None),
    ('cart_rolling', [['ZzzzzzzzzzzZ',
                       'ZwWwwwwWwwwZ',
                       'ZwwwWwwwwWwZ',
                       'ZWwwwwWwwwwZ',
                       'ZZZZZZZZZZZZ',
                       '.ZKZ....ZKZ.',
                       '..Z......Z..'],
                      ['ZzzzzzzzzzzZ',
                       'ZwWwwwwWwwwZ',
                       'ZwwwWwwwwWwZ',
                       'ZWwwwwWwwwwZ',
                       'ZZZZZZZZZZZZ',
                       '.KZK....KZK.',
                       '.Z.Z....Z.Z.']], 10, False, None),
    ('buffer', [['rfrfrfrf',
                 'frfrfrfr',
                 'WwwwwwwW',
                 'WwlwwlwW',
                 '.W....W.',
                 '.W....W.',
                 'WWW..WWW']], 0, False, None),
    ('mine_frame', [['WWWWWWWWWWWWWWWWWWWW',
                     'WwwlwwwwwwwwwwwlwwwW',
                     '.Ww..............wW.',
                     '.Ww..............wW.',
                     '.Ww..............wW.',
                     '.Ww..............wW.',
                     '.Ww..............wW.',
                     '.Ww..............wW.',
                     '.Ww..............wW.',
                     '.Ww..............wW.',
                     '.Ww..............wW.',
                     '.Ww..............wW.',
                     '.Ww..............wW.',
                     'WWWW............WWWW']], 0, False, None),
    # The stopwatch's posts: a green ribbon where a timed run starts, a chequered flag where it ends.
    ('trial_start', [['8888.',
                      '88888',
                      '8888.',
                      'W....',
                      'W....',
                      'W....',
                      'W....',
                      'W....',
                      'WW...']], 0, False, None),
    ('trial_finish', [['KvKvK.',
                       'vKvKvK',
                       'KvKvK.',
                       'W.....',
                       'W.....',
                       'W.....',
                       'W.....',
                       'W.....',
                       'WW....'],
                      ['.KvKvK',
                       'vKvKv.',
                       '.KvKvK',
                       'W.....',
                       'W.....',
                       'W.....',
                       'W.....',
                       'W.....',
                       'WW....']], 3, False, None),
    # A beacon on a hilltop, one at the end of each route: cold until you've been there.
    ('beacon_off', [['..........',
                     '..........',
                     '..........',
                     '..........',
                     '..KkaKk...',
                     '.sSsSsSsS.',
                     '..sSsSsS..',
                     '...W..W...',
                     '..W....W..',
                     '.W......W.']], 0, False, None),
    ('beacon', flame_frames(['.sSsSsSsS.',
                             '..sSsSsS..',
                             '...W..W...',
                             '..W....W..',
                             '.W......W.'],
                            [['....y.....', '...yoy....', '..yorroy..', '..orRro...', '..WwlwW...'],
                             ['...y......', '...oyy....', '..yoroy...', '..orRrro..', '..WwlwW...'],
                             ['.....y....', '....yoy...', '..yorrroy.', '..oRrRo...', '..WwlwW...']]), 8, False, (255, 170, 90, 56)),
    # At the top of the old line: the Lantern, dark.
    ('great_lantern_off', [['.......ZZZZZZ.......',
                            '......ZzzzzzzZ......',
                            '.....ZZZZZZZZZZ.....',
                            '....Z.z..z..z..Z....',
                            '....Z.z..z..z..Z....',
                            '....ZuuuuuuuuuuZ....',
                            '....ZuUuuuuuUuuZ....',
                            '....ZuuuuuuuuuuZ....',
                            '....Z.z..z..z..Z....',
                            '....Z.z..z..z..Z....',
                            '...ZZZZZZZZZZZZZZ...',
                            '....sSsSsSsSsSsS....',
                            '....SsSsSsSsSsSs....',
                            '....sSsSsSsSsSsS....',
                            '...sSsSsSsSsSsSsS...',
                            '..sSsSsSsSsSsSsSsS..',
                            '.sSsSsSsSsSsSsSsSsS.',
                            'sSsSsSsSsSsSsSsSsSsS']], 0, False, None),
    # The Still: a cluster of stars come down to rest, and the rift the waterfall pours from.
    ('star_cluster', [['...6.....',
                       '..616..3.',
                       '...6..343',
                       '.3.....3.',
                       '343..6...',
                       '.3..616..',
                       '.....6...']], 0, False, (200, 210, 255, 26)),
    ('rift', [['..3..', '.343.', '.363.', '34643', '.363.', '.343.', '..3..'],
              ['..3..', '.363.', '.343.', '36463', '.343.', '.363.', '..3..'],
              ['..4..', '.343.', '.363.', '34643', '.363.', '.343.', '..4..']], 4, False, (200, 160, 255, 34)),
]


def make_props():
    sheet_w = 256
    placed, x, y, row_h = [], 0, 0, 0
    for name, frames, fps, hangs, light in PROPS:
        width = max(len(r) for r in frames[0])
        grids = [outlined_prop([r.ljust(width, '.') for r in f]) for f in frames]
        fw, fh = len(grids[0][0]), len(grids[0])
        if x + fw * len(grids) > sheet_w:
            x, y, row_h = 0, y + row_h, 0
        placed.append((name, x, y, fw, fh, grids, fps, hangs, light))
        x += fw * len(grids)
        row_h = max(row_h, fh)
    c = Canvas(sheet_w, y + row_h)
    lines = ['# What each prop in props.png is: name x y width height frames fps hangs light_r light_g light_b light_radius',
             '# Made by tools/make_art.py with props.png.']
    for name, px, py, fw, fh, grids, fps, hangs, light in placed:
        for i, g in enumerate(grids):
            c.blit(g, PROP, px + i * fw, py)
        r, gr, b, radius = light or (0, 0, 0, 0)
        lines.append(f'{name} {px} {py} {fw} {fh} {len(grids)} {fps} {int(hangs)} {r} {gr} {b} {radius}')
    c.save('props.png')
    with open(os.path.join(OUT, 'props.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('  props:', ', '.join(p[0] for p in placed))


def outlined_prop(grid):
    """A 1px outline round the drawing, which grows by a pixel on every side."""
    w = len(grid[0])
    g = ['.' * (w + 2)] + ['.' + r + '.' for r in grid] + ['.' * (w + 2)]
    out = [list(r) for r in g]
    for yy in range(len(g)):
        for xx in range(w + 2):
            if g[yy][xx] == '.' and any(0 <= xx + dx < w + 2 and 0 <= yy + dy < len(g) and g[yy + dy][xx + dx] != '.'
                                        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[yy][xx] = '#'
    return [''.join(r) for r in out]


# Ice: the Frost Caves. Deep blue rock under frost, icicles, and ice pillars glowing in the dark.
ICE_ROCK = {'0': rgb('#0e1828'), '1': rgb('#1e3048'), '2': rgb('#2a4462'), '3': rgb('#3c5c82'),
            'g': rgb('#6aa0d0'), 'G': rgb('#a8d4f4'), 'h': rgb('#dcf2ff'), 'H': rgb('#ffffff'),
            't': rgb('#4a90c8'), 'T': rgb('#8ad0ff'), 'w': rgb('#5a6a88')}
frost_top = ['HHhHHHhH',
             'hGhhGhhh',
             'GgGG3GgG',
             'g.g...g.',
             '........',
             '........',
             '........',
             '........']
frost_top_b = ['HhHHHHHh',
               'hhGhhGhG',
               'GGgG3gGG',
               '.g..g..g',
               '........',
               '........',
               '........',
               '........']
ice_shard = ['........',
             '........',
             '...T....',
             '..TT..T.',
             '..Tt.TT.',
             '.tTt.Tt.',
             '.tTttTt.',
             'ttttttt.']
frost_drip = ['3gg3gg3.',
              '.gT.Tg..',
              '..T..T..',
              '..t..T..',
              '.....t..',
              '........',
              '........',
              '........']


def make_ice():
    make_tiles('ice', tile_sheet(ICE_ROCK, (frost_top, frost_top_b), (ice_shard, ice_shard, frost_drip), IRON))
    c = sky('#0c1424', '#16263c')
    stars(c, count=40, seed=3, color='#8ad0ff', below=H)
    c.save('themes/ice/sky.png')
    c = cave_layer('#162438', top_depth=30, bottom_height=40, seed=2.7, jag=1)
    stalactites(c, '#1c3050', seed=4, count=26, length=(20, 60))
    c.save('themes/ice/far.png')
    c = cave_layer('#101a2c', top_depth=18, bottom_height=26, seed=6.3, jag=2)
    stalactites(c, '#2a5a8a', seed=11, count=14, length=(10, 34))
    c.save('themes/ice/near.png')


# Sky: pale stone islands among the clouds, high above everything, bright and windy.
SKY_ROCK = {'0': rgb('#262c48'), '1': rgb('#3a4264'), '2': rgb('#4e5880'), '3': rgb('#6c76a0'),  # moonlit
            'g': rgb('#2a5a4c'), 'G': rgb('#3e7c5c'), 'h': rgb('#6ab884'), 'H': rgb('#b4ecc4'),
            'f': rgb('#e8eeff'), 'p': rgb('#f0d060'), 'w': rgb('#5a4a42')}
cloud_moss = ['HhHHhHHh',
              'hGhhGhhG',
              'GgGG3GgG',
              '3g3333g3',
              '3.3..3.3',
              '........',
              '........',
              '........']
cloud_moss_b = ['hHhHHHhH',
                'GhhGhhGh',
                'gGG3GGgG',
                '33g33g33',
                '.3..3..3',
                '........',
                '........',
                '........']
sky_tuft = ['........',
            '........',
            '........',
            '........',
            '...f....',
            '..fpf.h.',
            '...H.hH.',
            '..HhHhH.']
sky_wisp = ['........',
            '........',
            '........',
            '........',
            '........',
            '.ff.....',
            'fffff.f.',
            '.fffffff']


def make_sky():
    # Up above the camp, the same night: the moon nearer, the stars thick, the aurora below, and
    # clouds and islands silvered by the moonlight. No daylight anywhere tonight.
    make_tiles('sky', tile_sheet(SKY_ROCK, (cloud_moss, cloud_moss_b), (sky_tuft, sky_wisp, roots)))
    c = sky('#060a24', '#24346a')
    stars(c, count=190, seed=51, color='#ffffff', below=150)
    stars(c, count=40, seed=52, color='#9ab8ff', below=170)
    for x in range(W):  # the aurora, low now: you're above it
        top = int(128 + 8 * periodic_noise(x, 1.1, ((1, 1.0), (2, 0.6), (5, 0.2))))
        for dy in range(24):
            fade = 1 - dy / 24
            c.mix(x, top + dy, rgb('#5af0b0' if dy < 12 else '#a070ff', int(22 * fade * (0.6 + 0.4 * math.sin(x * 0.09 + dy * 0.2)))))
    glow(c, 262, 30, 10, '#f4f0d8', rings=5)
    for x, y, size in ((40, 30, 12), (120, 52, 16), (210, 20, 9), (290, 70, 14), (70, 96, 10), (180, 110, 13)):
        cloud(c, x, y, size, '#8c9cc8', '#56689a')
    c.save('themes/sky/sky.png')
    c = Canvas(W, H)
    rng = random.Random(21)
    for _ in range(9):  # far islands, hanging in the dark
        cx, cy, w = rng.randrange(W), rng.randrange(60, 130), rng.randrange(14, 34)
        for y in range(10):
            half = w * (1 - y / 10) * 0.5 + 2
            for x in range(int(cx - half), int(cx + half) + 1):
                c.put(x % W, cy + y, rgb('#2c3862'))
        for x in range(cx - w // 2, cx + w // 2 + 1):
            c.put(x % W, cy - 1, rgb('#3e6a64'))
    c.save('themes/sky/far.png')
    c = Canvas(W, H)
    for x in range(W):  # a bank of cloud below, lit from above
        top = int(H - 30 - 10 * periodic_noise(x, 8.1))
        for y in range(top, H):
            c.put(x, y, rgb('#7a8abc' if y < top + 3 else '#3c4a7c'))
    c.save('themes/sky/near.png')


# Grotto: the Hidden Lake, where it all starts. Deep blue rock under glowing moss, bellflowers and
# roots tipped with light, crystals glinting in the far walls, and magic drifting in the air.
GROTTO = {'0': rgb('#121629'), '1': rgb('#1f2a44'), '2': rgb('#2a3a5c'), '3': rgb('#3d5078'),
          'g': rgb('#1d5058'), 'G': rgb('#2a8a86'), 'h': rgb('#4fd8c8'), 'H': rgb('#b4fff2'),
          'f': rgb('#fff3a0'), 'p': rgb('#b08aff'), 'P': rgb('#6a4ac0'), 'r': rgb('#2a3a5c')}
grotto_bell = ['........',
               '........',
               '...p....',
               '..pfp.p.',
               '..PpP.pP',
               '...G..G.',
               '..hG.Gh.',
               '.GhGhG..']
grotto_drip = ['0110110.',
               '.01.10..',
               '..1..0..',
               '..h..1..',
               '.....h..',
               '........',
               '........',
               '........']


def make_grotto():
    make_tiles('grotto', tile_sheet(GROTTO, (glow_moss, glow_moss_b), (sprouts, grotto_bell, grotto_drip)))
    c = sky('#0b0f22', '#122a3a')
    stars(c, count=70, seed=31, color='#9af0ff', below=H)
    sparks(c, seed=32, count=28, colors=('#7af0e0', '#c8a0ff', '#fff0a0'), above=0)
    c.save('themes/grotto/sky.png')
    c = cave_layer('#18233c', top_depth=34, bottom_height=40, seed=3.7, jag=1)
    stalactites(c, '#18233c', seed=33, count=20, length=(16, 50))
    # Crystals glinting in the far walls.
    rng = random.Random(34)
    for _ in range(16):
        cx, cy = rng.randrange(W), rng.choice((rng.randrange(10, 36), rng.randrange(150, 172)))
        col = rgb(rng.choice(('#3a9ab0', '#7a5ac0', '#4fd8c8')))
        for i in range(rng.randrange(3, 8)):
            c.put(cx % W, cy - i, col)
            if i < 4:
                c.put((cx + 2) % W, cy - i, col)
    c.save('themes/grotto/far.png')
    c = cave_layer('#0f1528', top_depth=20, bottom_height=24, seed=7.9, jag=2)
    vines(c, '#0f1528', '#2a8a86', seed=35, count=22, length=(8, 46))
    c.save('themes/grotto/near.png')


# Glade: out in the open at night, by the fire. Dark earth under blue-green grass, glowing flowers,
# hills and pines against the stars, and an aurora rippling over them.
GLADE = {'0': rgb('#14141f'), '1': rgb('#242536'), '2': rgb('#32344a'), '3': rgb('#46496a'),
         'g': rgb('#1d4a3a'), 'G': rgb('#2c7050'), 'h': rgb('#58b878'), 'H': rgb('#a8f0b4'),
         'f': rgb('#fff0a0'), 'p': rgb('#8ad8ff')}
glade_flower = ['........',
                '........',
                '....p...',
                '...pfp..',
                '.p..p...',
                'pfp.G...',
                '.G..G.h.',
                '.GhGhGG.']


def make_glade():
    make_tiles('glade', tile_sheet(GLADE, decor=(tuft, glade_flower, vine)))
    c = sky('#070b22', '#1a2850')
    stars(c, count=140, seed=41, color='#ffffff', below=120)
    # The aurora: a rippling band of green fading down into violet.
    for x in range(W):
        top = int(28 + 10 * periodic_noise(x, 1.1, ((1, 1.0), (2, 0.6), (5, 0.2))))
        for dy in range(30):
            fade = 1 - dy / 30
            c.mix(x, top + dy, rgb('#5af0b0' if dy < 16 else '#a070ff', int(28 * fade * (0.6 + 0.4 * math.sin(x * 0.09 + dy * 0.2)))))
    glow(c, 262, 30, 7, '#f4f0d8', rings=4)
    c.save('themes/glade/sky.png')
    c = Canvas(W, H)
    fill_up(c, rgb('#1a2240'), peaks(seed=42, count=7, height=(70, 115), slope=(0.5, 1.0)))
    c.save('themes/glade/far.png')
    c = Canvas(W, H)
    ground = lambda x: 24 + 8 * periodic_noise(x, 6.1)
    tree_line(c, '#0e1424', ground, seed=43, spacing=26, radius=(8, 14))
    pines(c, '#0e1424', ground, seed=44, spacing=34, height=(30, 52))
    sparks(c, seed=45, count=22, colors=('#e8f08c', '#8ad8ff'), above=70)  # fireflies
    c.save('themes/glade/near.png')


# The Still: behind the waterfall, the place between the stars it pours from. No ground, no sky:
# black all round, stars near and far, and stone the colour of the night floating in it, crusted
# with starlight.
STILL = {'0': rgb('#07060d'), '1': rgb('#110e22'), '2': rgb('#1b1736'), '3': rgb('#2a2450'),
         'g': rgb('#2a2a5a'), 'G': rgb('#4a4aa0'), 'h': rgb('#8a8ae8'), 'H': rgb('#d8d8ff'),
         'f': rgb('#fff6d0'), 'p': rgb('#b89aff')}
still_moss = ['H..h..H.',
              'hGhhGhhG',
              'GgGGgGgG',
              'g1gg1g1g',
              '.g...g..',
              '........',
              '........',
              '........']
still_moss_b = ['.H...h.H',
                'GhGhhGhG',
                'gGgGGgGg',
                '1g1gg1g1',
                '..g...g.',
                '........',
                '........',
                '........']
still_star = ['........',
              '........',
              '....f...',
              '...fHf..',
              '....f...',
              '........',
              '........',
              '........']
still_sprout = ['........',
                '........',
                '........',
                '........',
                '........',
                '..h.....',
                '..G..h..',
                '.GhG.G..']
still_drip = ['0001000.',
              '.00100..',
              '..h10...',
              '...0....',
              '...h....',
              '........',
              '........',
              '........']


def make_still():
    make_tiles('still', tile_sheet(STILL, (still_moss, still_moss_b), (still_sprout, still_star, still_drip)))
    c = sky('#020208', '#05040c')
    stars(c, count=170, seed=91, color='#ffffff')
    stars(c, count=40, seed=92, color='#b8c8ff')
    c.save('themes/still/sky.png')
    # Far: faint drifts of stars, and a thin veil of violet across them.
    c = Canvas(W, H)
    for x in range(W):
        mid = int(90 + 30 * periodic_noise(x, 9.1, ((1, 1.0), (2, 0.4))))
        for dy in range(-14, 15):
            c.mix(x, mid + dy, rgb('#5a3a9a', int(10 * (1 - abs(dy) / 15))))
    stars(c, count=60, seed=93, color='#d8d0ff')
    c.save('themes/still/far.png')
    # Near: rocks adrift in the dark, black on black, a star glinting on one now and then.
    c = Canvas(W, H)
    rng = random.Random(94)
    for _ in range(9):
        cx, cy, r = rng.randrange(W), rng.randrange(10, H - 10), rng.uniform(3, 9)
        disc(c, cx, cy, r, rgb('#0c0a18'))
        disc(c, cx + r * 0.3, cy - r * 0.6, r * 0.5, rgb('#0c0a18'))
        c.put(int(cx - r * 0.4) % W, int(cy - r * 0.5), rgb('#8a8ae8', 160))
    c.save('themes/still/near.png')


# The Mine: the old line up the mountain. Brown rock propped up with timbers, gravel and ore in it,
# and outside, snowy peaks and pines under the night.
MINE = {'0': rgb('#1a1612'), '1': rgb('#2c251e'), '2': rgb('#3e3428'), '3': rgb('#554636'),
        'g': rgb('#4a3e30'), 'G': rgb('#6a5a44'), 'h': rgb('#94805e'), 'H': rgb('#c4ae86'),
        'f': rgb('#e8c84a'), 'p': rgb('#7ad0e8')}
gravel_top = ['HhHhHHhH',
              'hGhGhhGh',
              'GgGgGGgG',
              'g1gg1g1g',
              '.g...g..',
              '........',
              '........',
              '........']
gravel_top_b = ['hHhHhhHh',
                'GhGhGhGh',
                'gGgGgGgG',
                '1g1gg1g1',
                '..g...g.',
                '........',
                '........',
                '........']
ore = ['........',
       '........',
       '........',
       '........',
       '........',
       '...f....',
       '..ffG...',
       '.GgGgG..']
pebbles = ['........',
           '........',
           '........',
           '........',
           '........',
           '........',
           '.h...G..',
           'GhG.GhG.']


def make_mine():
    make_tiles('mine', tile_sheet(MINE, (gravel_top, gravel_top_b), (pebbles, ore, drip)))
    c = sky('#0a0e22', '#232846')
    stars(c, count=110, seed=81, color='#ffffff', below=110)
    glow(c, 60, 34, 6, '#e8e4d0', rings=3)
    c.save('themes/mine/sky.png')
    c = Canvas(W, H)
    snowy_peaks(c, '#1c2036', '#3e4462', peaks(seed=82, count=7, height=(70, 120), slope=(0.8, 1.4)), snowline=14)
    c.save('themes/mine/far.png')
    c = Canvas(W, H)
    ground = lambda x: 30 + 10 * periodic_noise(x, 8.3)
    fill_up(c, rgb('#12141e'), ground)
    pines(c, '#12141e', ground, seed=83, spacing=22, height=(26, 48))
    c.save('themes/mine/near.png')


THEMES = [make_cave, make_garden, make_snow, make_ember, make_daylight, make_ruins, make_ice, make_sky, make_grotto, make_glade,
          make_still, make_mine]


if __name__ == '__main__':
    for make_theme in THEMES:
        make_theme()
    make_player()
    make_objects()
    make_items()
    make_font()
    make_bench()
    make_props()
