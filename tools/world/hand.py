"""The rooms made by hand, in the room editor (tools/editor.cpp): a file each in tools/world/rooms/,
laid out as src/RoomFile.h says. Each is made into a Room like the region files' rooms, its doors
found from the gaps in its outside walls; its joins say which of them lead where, by where the
doors start. tools/make_world.py puts them in the world."""
import os
from world.build import Room

FOLDER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rooms')
SOLID = set('#%o~&=m')  # as isSolidChar in src/RoomFile.cpp


def doors(r):
    """The gaps in the outside walls, as door() records them: side -> [(start, length)]. Not the
    corners, which are both sides' walls."""
    found = {}
    for side, length, tile in (('L', r.h, lambda i: r.g[i][0]), ('R', r.h, lambda i: r.g[i][r.w - 1]),
                               ('B', r.w, lambda i: r.g[0][i]), ('T', r.w, lambda i: r.g[r.h - 1][i])):
        start = None
        for i in range(1, length):
            open_ = i < length - 1 and tile(i) not in SOLID
            if open_ and start is None:
                start = i
            if not open_ and start is not None:
                found.setdefault(side, []).append((start, i - start))
                start = None
    return found


def read(path):
    """A room file: the Room, and its joins as (room, side, start, other room, side, start)."""
    name = os.path.splitext(os.path.basename(path))[0]
    where = os.path.relpath(path)
    title, theme, settings, joins, fields, rows = name, 'cave', {}, [], {}, None
    with open(path) as f:
        for n, line in enumerate(f.read().splitlines(), 1):
            if rows is not None:
                rows.append(line)
                continue
            words = line.split()
            if not words or words[0].startswith('#'):
                continue
            key, rest = words[0], line.strip()[len(words[0]):].strip()
            if key == 'tiles':
                rows = []
            elif key == 'title':
                title = rest
            elif key == 'theme':
                theme = rest
            elif key in ('region', 'wind', 'dark', 'space'):
                settings[key] = rest
            elif key == 'join':
                side, start, other, other_side, other_start = rest.split()
                joins.append((name, side, int(start), other, other_side, int(other_start)))
            elif key == 'entity':
                x, y, *pairs = rest.split()
                fields[(int(x), int(y))] = dict(p.split('=', 1) for p in pairs)
            else:
                raise SystemExit(f'{where} line {n}: don\'t know "{key}"')
    while rows and not rows[-1]:
        rows.pop()
    if not rows:
        raise SystemExit(f'{where} has no tiles')
    if any(len(row) != len(rows[0]) for row in rows):
        raise SystemExit(f"{where}: the rows of tiles aren't all as long")
    r = Room(name, title, theme, len(rows[0]), len(rows))
    r.g = [list(row) for row in reversed(rows)]
    r.starts = sum(row.count('S') for row in r.g)  # one, to go in the world
    for (x, y) in fields:
        if not (0 <= x < r.w and 0 <= y < r.h) or r.g[y][x] not in 'SCGP*NBLDYURQ':
            raise SystemExit(f"{where}: the entity at {x} {y} isn't on a thing's tile")
    r.fields = fields
    r.settings = settings
    r.doors = doors(r)
    return r, joins


def problem(r):
    """What stops a room going in the world, if anything: no Start, or an open corner (a door is in
    one wall, and a gap in a corner would lead nowhere)."""
    if r.starts != 1:
        return 'it needs one Start (S)' if r.starts == 0 else f'it has {r.starts} Starts (S): one, please'
    for x, y, name in ((0, 0, 'bottom-left'), (r.w - 1, 0, 'bottom-right'), (0, r.h - 1, 'top-left'), (r.w - 1, r.h - 1, 'top-right')):
        if r.g[y][x] not in SOLID:
            return f'its {name} corner is open: corners have to be rock'
    return None


def rooms():
    """Every room in the folder, by file name: (Room, joins)."""
    if not os.path.isdir(FOLDER):
        return []
    return [read(os.path.join(FOLDER, f)) for f in sorted(os.listdir(FOLDER)) if f.endswith('.txt')]
