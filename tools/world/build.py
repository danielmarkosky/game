"""What the rooms are made with: a Room, drawn tile by tile with y up, and a few helpers. Each
region's file (tools/world/<region>.py) makes its rooms with these."""


class Room:
    def __init__(self, name, title, theme, w, h):
        self.name, self.title, self.theme, self.w, self.h = name, title, theme, w, h
        self.g = [['.'] * w for _ in range(h)]
        self.doors = {}   # side -> [(start, length)]
        self.fields = {}  # (x, y) -> {field: value}
        self.settings = {}  # wind, dark, region: see tools/rooms.py
        self.fill(0, w - 1, 0, 0, '#')
        self.fill(0, w - 1, h - 1, h - 1, '#')
        self.fill(0, 0, 0, h - 1, '#')
        self.fill(w - 1, w - 1, 0, h - 1, '#')

    def fill(self, x0, x1, y0, y1, c):
        for x in range(min(x0, x1), max(x0, x1) + 1):
            for y in range(min(y0, y1), max(y0, y1) + 1):
                self.g[y][x] = c
        return self

    def put(self, x, y, c, **fields):
        self.g[y][x] = c
        if fields:
            self.fields[(x, y)] = fields
        return self

    def prop(self, x, y, kind, needs=None):
        """Scenery (a prop in assets/gfx/props.txt, or light_shaft), in an empty tile: standing on
        its bottom, or hanging from its top. With `needs`, drawn as its "_off" picture until that
        fact holds (a beacon, unlit until you've the ability it's for)."""
        assert self.g[y][x] == '.', (self.name, x, y, kind, 'not empty')
        return self.put(x, y, 'D', kind=kind, **({'needs': needs} if needs else {}))

    def props(self, *placed):
        for x, y, kind in placed:
            self.prop(x, y, kind)
        return self

    def door(self, side, start, length=4):
        """A gap in an outside wall: L and R count `start` up from the bottom, T and B from the left."""
        for i in range(length):
            if side == 'L': self.g[start + i][0] = '.'
            if side == 'R': self.g[start + i][self.w - 1] = '.'
            if side == 'B': self.g[0][start + i] = '.'
            if side == 'T': self.g[self.h - 1][start + i] = '.'
        self.doors.setdefault(side, []).append((start, length))
        return self

    def text(self):
        return '\n'.join(''.join(self.g[y]) for y in range(self.h - 1, -1, -1)) + '\n'


ROOMS = {}  # every room made so far, by name, in the order they were made
_region = None


def region(name):
    """The region the rooms made from now on are in: one of assets/regions.txt."""
    global _region
    _region = name


def room(*args):
    assert _region, 'say which region first'
    r = Room(*args)
    assert r.name not in ROOMS, (r.name, 'made twice')
    r.settings['region'] = _region
    ROOMS[r.name] = r
    return r


def ground(r, top, x0=0, x1=None):
    """Rock from the bottom up to `top`: stand at top + 1."""
    return r.fill(x0, r.w - 1 if x1 is None else x1, 0, top, '#')


def roof(r, bottom, x0=0, x1=None):
    return r.fill(x0, r.w - 1 if x1 is None else x1, bottom, r.h - 1, '#')


def pit(r, x0, x1, bottom=1, top=4, spikes=True):
    r.fill(x0, x1, bottom, top, '.')
    if spikes:
        r.fill(x0, x1, bottom, bottom, '^')
    return r
