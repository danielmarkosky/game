#!/usr/bin/env python3
"""Moves the world between the LDtk project (assets/world.ldtk) and plain text rooms.

The LDtk project is what the game loads and what you edit in LDtk (https://ldtk.io). Text is
handy for big edits with a script or a text editor, and for looking at a room in a diff:

  python3 tools/rooms.py export assets/world.ldtk some_dir/   # every room as text, plus rooms.txt
  python3 tools/rooms.py import some_dir/rooms.txt assets/world.ldtk

Text rooms use the characters in assets/levels/README.md. rooms.txt lists them like this:

  theme cave                                   # the theme of the rooms below it
  level1   0 0  The Cave                       # file (next to rooms.txt), map position, title
  entity level4 142 25 ability=long_rope      # fields of the entity at column 142, row 25
  room level7 wind=-60 dark=1                  # a room's settings: wind (px/s each second), dark

Positions are in tiles, of the room's bottom-left tile, x to the right and y up. Rows count from
the top of the room, like lines of the text file. Importing into an existing project keeps its
definitions and the ids of rooms and entities that are still there, so LDtk diffs stay small.
"""
import json
import os
import sys
import uuid

GRID = 8

# IntGrid values of the Tiles layer, and the text characters for them.
TILES = [(1, '#', 'rock', '#6A5C7A'), (2, '%', 'hook_block', '#A6B6CC'), (3, '-', 'platform', '#B47C48'),
         (4, '^', 'spikes', '#EEF1F8'), (5, 'x', 'crystal', '#E5487A'), (6, '=', 'gate', '#8899AA'), (7, 'o', 'slick', '#9AC0E0'),
         (8, '~', 'ice', '#CFEFFF'), (9, '&', 'crumble', '#A8886C'), (10, '_', 'cloud', '#FFFFFF'),
         (11, 'w', 'water', '#3A8FD0'), (12, 'm', 'bounce', '#E86A9A'), (13, '?', 'hidden', '#5A4E6A'),
         (14, 't', 'rail', '#8A7A6A')]
CHAR_OF = {value: char for value, char, _, _ in TILES}
VALUE_OF = {char: value for value, char, _, _ in TILES}

THEMES = ['cave', 'garden', 'snow', 'ember', 'daylight', 'ruins', 'ice', 'sky', 'grotto', 'glade', 'still', 'mine']
ABILITIES = ['rope', 'long_rope', 'wall_climb', 'air_throw', 'double_jump', 'stopwatch', 'tie_off']

# Entities: identifier, text character, colour, at most one per room, fields.
ENTITIES = [
    ('Start', 'S', '#63C74D', True, []),
    ('Checkpoint', 'C', '#7EF08E', False, []),
    ('Goal', 'G', '#FFD85A', False, []),
    ('Pickup', 'P', '#E8B94A', False, [('ability', 'Ability')]),
    ('Gem', '*', '#4FD0E8', False, []),
    ('Npc', 'N', '#D77BBA', False, [('dialogue', 'String'), ('look', 'String'), ('pose', 'String'), ('needs', 'String'), ('until', 'String')]),
    ('Bench', 'B', '#B47C48', False, []),
    ('Lever', 'L', '#E8443A', False, []),
    ('Prop', 'D', '#8A8AA0', False, [('kind', 'String'), ('needs', 'String')]),
    ('Icicle', 'Y', '#CFEFFF', False, []),
    ('Updraft', 'U', '#FF8C3C', False, []),
    ('Ring', 'R', '#E8B94A', False, [('to', 'String'), ('period', 'String')]),
    ('Lamp', 'Q', '#FFD86A', False, []),
    ('Cart', 'K', '#8A7A6A', False, [('dir', 'String'), ('needs', 'String')]),
    ('CartStop', 'E', '#6A5A4A', False, []),
    ('Trial', 'T', '#7EF08E', False, [('route', 'String'), ('mark', 'String')]),
    ('Stone', 'O', '#968C96', False, []),
    ('Plate', 'V', '#78C8AA', False, []),
    ('Fire', 'F', '#FF8C3C', False, [('period', 'String'), ('phase', 'String'), ('flue', 'String')]),
]
ENTITY_OF_CHAR = {char: ident for ident, char, *_ in ENTITIES}
CHAR_OF_ENTITY = {ident: char for ident, char, *_ in ENTITIES}
DEFAULT_FIELDS = {'Pickup': {'ability': 'rope'}}

# Fixed uids, so the definitions are the same in every project this makes.
UID = {'Tiles': 1, 'Entities': 2, 'Theme': 3, 'Ability': 4, 'title': 5, 'theme': 6, 'wind': 7, 'dark': 8, 'region': 9, 'space': 100}
# A room's own settings besides its title and theme: in rooms.txt as "room <name> wind=40 dark=1".
ROOM_FIELDS = ['wind', 'dark', 'region', 'space']
for n, (ident, *_rest) in enumerate(ENTITIES):
    UID[ident] = 10 + n
FIELD_UID = {}
for n, (ident, _, _, _, fields) in enumerate(ENTITIES):
    for m, (name, _) in enumerate(fields):
        # Four to an entity at first; more than that, further on (so the first four keep their uids).
        FIELD_UID[(ident, name)] = 30 + 4 * n + m if m < 4 else 300 + 8 * n + m


def new_iid():
    return str(uuid.uuid4())


# ----------------------------------------------------------------------------------------------
# Definitions
# ----------------------------------------------------------------------------------------------

def field_type(kind):
    if kind == 'String':
        return 'String', 'F_String'
    return f'LocalEnum.{kind}', f'F_Enum({UID[kind]})'


def field_def(name, kind, uid):
    t, internal = field_type(kind)
    return {
        'identifier': name, 'doc': None, '__type': t, 'uid': uid, 'type': internal, 'isArray': False,
        'canBeNull': False, 'arrayMinLength': None, 'arrayMaxLength': None, 'editorDisplayMode': 'NameAndValue',
        'editorDisplayScale': 1, 'editorDisplayPos': 'Above', 'editorLinkStyle': 'StraightArrow',
        'editorDisplayColor': None, 'editorAlwaysShow': False, 'editorShowInWorld': True, 'editorCutLongValues': True,
        'editorTextSuffix': None, 'editorTextPrefix': None, 'useForSmartColor': False, 'exportToToc': False,
        'searchable': False, 'min': None, 'max': None, 'regex': None, 'acceptFileTypes': None,
        'defaultOverride': None, 'textLanguageMode': None, 'symmetricalRef': False, 'autoChainRef': False,
        'allowOutOfLevelRef': True, 'allowedRefs': 'OnlySame', 'allowedRefsEntityUid': None,
        'allowedRefTags': [], 'tilesetUid': None,
    }


def layer_def(ident, kind, uid):
    return {
        '__type': kind, 'identifier': ident, 'type': kind, 'uid': uid, 'doc': None, 'uiColor': None,
        'gridSize': GRID, 'guideGridWid': 0, 'guideGridHei': 0, 'displayOpacity': 1, 'inactiveOpacity': 0.6,
        'hideInList': False, 'hideFieldsWhenInactive': True, 'canSelectWhenInactive': True,
        'renderInWorldView': True, 'pxOffsetX': 0, 'pxOffsetY': 0, 'parallaxFactorX': 0, 'parallaxFactorY': 0,
        'parallaxScaling': True, 'requiredTags': [], 'excludedTags': [], 'autoTilesKilledByOtherLayerUid': None,
        'uiFilterTags': [], 'useAsyncRender': False,
        'intGridValues': [{'value': v, 'identifier': name, 'color': color, 'tile': None, 'groupUid': 0}
                          for v, _, name, color in TILES] if kind == 'IntGrid' else [],
        'intGridValuesGroups': [], 'autoRuleGroups': [], 'autoSourceLayerDefUid': None, 'tilesetDefUid': None,
        'tilePivotX': 0, 'tilePivotY': 0, 'biomeFieldUid': None,
    }


def entity_def(ident, color, one_per_room, fields):
    return {
        'identifier': ident, 'uid': UID[ident], 'tags': [], 'exportToToc': False, 'allowOutOfBounds': False,
        'doc': None, 'width': GRID, 'height': GRID, 'resizableX': False, 'resizableY': False, 'minWidth': None,
        'maxWidth': None, 'minHeight': None, 'maxHeight': None, 'keepAspectRatio': False, 'tileOpacity': 1,
        'fillOpacity': 0.6, 'lineOpacity': 1, 'hollow': False, 'color': color, 'renderMode': 'Rectangle',
        'showName': True, 'tilesetId': None, 'tileRenderMode': 'FitInside', 'tileRect': None, 'uiTileRect': None,
        'nineSliceBorders': [], 'maxCount': 1 if one_per_room else 0, 'limitScope': 'PerLevel',
        'limitBehavior': 'MoveLastOne', 'pivotX': 0, 'pivotY': 0,
        'fieldDefs': [field_def(name, kind, FIELD_UID[(ident, name)]) for name, kind in fields],
    }


def enum_def(ident, values):
    palette = [0x6A5C7A, 0x3F9446, 0xC2D4EA, 0xE8662A, 0x56B446]
    return {'identifier': ident, 'uid': UID[ident], 'iconTilesetUid': None, 'externalRelPath': None,
            'externalFileChecksum': None, 'tags': [],
            'values': [{'id': v, 'tileRect': None, 'color': palette[i % len(palette)]} for i, v in enumerate(values)]}


def definitions():
    return {
        'layers': [layer_def('Entities', 'Entities', UID['Entities']), layer_def('Tiles', 'IntGrid', UID['Tiles'])],
        'entities': [entity_def(ident, color, one, fields) for ident, _, color, one, fields in ENTITIES],
        'tilesets': [],
        'enums': [enum_def('Theme', THEMES), enum_def('Ability', ABILITIES)],
        'externalEnums': [],
        'levelFields': [field_def('title', 'String', UID['title']), field_def('theme', 'Theme', UID['theme'])]
                       + [field_def(name, 'String', UID[name]) for name in ROOM_FIELDS],
    }


def new_project():
    return {
        '__header__': {'fileType': 'LDtk Project JSON', 'app': 'LDtk', 'doc': 'https://ldtk.io/json',
                       'schema': 'https://ldtk.io/files/JSON_SCHEMA.json', 'appAuthor': "Sebastien 'deepnight' Benard",
                       'appVersion': '1.5.3', 'url': 'https://ldtk.io'},
        'iid': new_iid(), 'jsonVersion': '1.5.3', 'appBuildId': 473702, 'nextUid': 100,
        'identifierStyle': 'Free', 'toc': [], 'worldLayout': 'Free', 'worldGridWidth': 256, 'worldGridHeight': 256,
        'defaultLevelWidth': 320, 'defaultLevelHeight': 184, 'defaultPivotX': 0, 'defaultPivotY': 0,
        'defaultGridSize': GRID, 'defaultEntityWidth': GRID, 'defaultEntityHeight': GRID, 'bgColor': '#1A1C2A',
        'defaultLevelBgColor': '#22263A', 'minifyJson': False, 'externalLevels': False, 'exportTiled': False,
        'simplifiedExport': False, 'imageExportMode': 'None', 'exportLevelBg': True, 'pngFilePattern': None,
        'backupOnSave': False, 'backupLimit': 10, 'backupRelPath': None, 'levelNamePattern': 'Level_%idx',
        'tutorialDesc': 'The rooms of the game. Paint the Tiles layer, place entities, and set each '
                        'room\'s title and theme in its level fields. Rooms touching on the world map '
                        'are joined wherever both have a gap in the shared edge.',
        'customCommands': [], 'flags': [], 'defs': definitions(), 'levels': [], 'worlds': [],
        'dummyWorldIid': new_iid(),
    }


def ensure_definitions(project):
    """Adds any definition this tool knows about that the project is missing."""
    defs, wanted = project['defs'], definitions()
    for key in ('layers', 'entities', 'enums', 'levelFields'):
        have = {d['identifier'] for d in defs[key]}
        defs[key] += [d for d in wanted[key] if d['identifier'] not in have]
    for entity in defs['entities']:
        want = next((e for e in wanted['entities'] if e['identifier'] == entity['identifier']), None)
        if want:
            have = {f['identifier'] for f in entity['fieldDefs']}
            entity['fieldDefs'] += [f for f in want['fieldDefs'] if f['identifier'] not in have]
    for layer in defs['layers']:
        want = next((l for l in wanted['layers'] if l['identifier'] == layer['identifier']), None)
        if want:
            have = {v['value'] for v in layer['intGridValues']}
            layer['intGridValues'] += [v for v in want['intGridValues'] if v['value'] not in have]
    for enum in defs['enums']:
        want = next((e for e in wanted['enums'] if e['identifier'] == enum['identifier']), None)
        if want:
            # The tool's own lists (themes, abilities): what it has, no more, no less.
            ids = [v['id'] for v in want['values']]
            have = {v['id'] for v in enum['values']}
            enum['values'] = [v for v in enum['values'] if v['id'] in ids] + [v for v in want['values'] if v['id'] not in have]
    used = [d['uid'] for key in ('layers', 'entities', 'enums', 'levelFields') for d in defs[key]]
    used += [f['uid'] for e in defs['entities'] for f in e['fieldDefs']]
    project['nextUid'] = max(project.get('nextUid', 0), max(used) + 1)


# ----------------------------------------------------------------------------------------------
# Rooms
# ----------------------------------------------------------------------------------------------

def field_instance(name, kind, uid, value):
    t, _ = field_type(kind)
    return {'__identifier': name, '__type': t, '__value': value, '__tile': None, 'defUid': uid,
            'realEditorValues': [{'id': 'V_String', 'params': [value]}]}


def field_value(instances, name, default=None):
    for f in instances:
        if f['__identifier'] == name:
            return f['__value']
    return default


def room_to_ldtk(room, old_level, level_uid, world_y):
    """A level in LDtk's format from a text room. Keeps old_level's ids where it can."""
    rows = room['rows']
    h, w = len(rows), max(len(r) for r in rows)
    rows = [r.ljust(w, '.') for r in rows]
    old_layers = {li['__identifier']: li for li in (old_level or {}).get('layerInstances', [])}
    old_entities = {(e['__identifier'], tuple(e['__grid'])): e
                    for e in old_layers.get('Entities', {}).get('entityInstances', [])}

    entities = []
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            ident = ENTITY_OF_CHAR.get(ch)
            if not ident:
                continue
            fields = dict(DEFAULT_FIELDS.get(ident, {}))
            fields.update(room['fields'].get((x, y), {}))
            kinds = next(f for i, _, _, _, f in ENTITIES if i == ident)
            old = old_entities.get((ident, (x, y)))
            entities.append({
                '__identifier': ident, '__grid': [x, y], '__pivot': [0, 0], '__tags': [], '__tile': None,
                '__smartColor': next(c for i, _, c, *_ in ENTITIES if i == ident),
                'iid': old['iid'] if old else new_iid(), 'width': GRID, 'height': GRID, 'defUid': UID[ident],
                'px': [x * GRID, y * GRID],
                'fieldInstances': [field_instance(name, kind, FIELD_UID[(ident, name)], fields.get(name, ''))
                                   for name, kind in kinds],
                '__worldX': room['x'] * GRID + x * GRID, '__worldY': world_y + y * GRID,
            })

    def layer(ident, kind, **content):
        old = old_layers.get(ident)
        return {'__identifier': ident, '__type': kind, '__cWid': w, '__cHei': h, '__gridSize': GRID,
                '__opacity': 1, '__pxTotalOffsetX': 0, '__pxTotalOffsetY': 0, '__tilesetDefUid': None,
                '__tilesetRelPath': None, 'iid': old['iid'] if old else new_iid(), 'levelId': level_uid,
                'layerDefUid': UID[ident], 'pxOffsetX': 0, 'pxOffsetY': 0, 'visible': True, 'optionalRules': [],
                'intGridCsv': content.get('intGridCsv', []), 'autoLayerTiles': [], 'seed': 1, 'overrideTilesetUid': None,
                'gridTiles': [], 'entityInstances': content.get('entityInstances', [])}

    level = dict(old_level or {})
    level.update({
        'identifier': room['name'], 'iid': (old_level or {}).get('iid') or new_iid(), 'uid': level_uid,
        'worldX': room['x'] * GRID, 'worldY': world_y, 'worldDepth': 0, 'pxWid': w * GRID, 'pxHei': h * GRID,
        '__bgColor': '#22263A', 'bgColor': None, 'useAutoIdentifier': False, 'bgRelPath': None, 'bgPos': None,
        'bgPivotX': 0.5, 'bgPivotY': 0.5, '__smartColor': '#ADADB5', '__bgPos': None, 'externalRelPath': None,
        'fieldInstances': [field_instance('title', 'String', UID['title'], room['title']),
                           field_instance('theme', 'Theme', UID['theme'], room['theme'])]
                          + [field_instance(name, 'String', UID[name], room['settings'].get(name, '')) for name in ROOM_FIELDS],
        'layerInstances': [layer('Entities', 'Entities', entityInstances=entities),
                           layer('Tiles', 'IntGrid', intGridCsv=[VALUE_OF.get(ch, 0) for row in rows for ch in row])],
        '__neighbours': [],
    })
    return level


def ldtk_to_room(level, bottom):
    """A text room from a level in LDtk's format. `bottom` is the lowest edge of any room, in px."""
    layers = {li['__identifier']: li for li in level['layerInstances']}
    tiles = layers['Tiles']
    w, h = tiles['__cWid'], tiles['__cHei']
    grid = [[CHAR_OF.get(tiles['intGridCsv'][y * w + x], '.') for x in range(w)] for y in range(h)]
    fields = {}
    for e in layers['Entities']['entityInstances']:
        x, y = e['__grid']
        grid[y][x] = CHAR_OF_ENTITY[e['__identifier']]
        values = {f['__identifier']: f['__value'] for f in e['fieldInstances']}
        if values and values != DEFAULT_FIELDS.get(e['__identifier']):
            fields[(x, y)] = values
    return {'name': level['identifier'], 'rows': [''.join(r) for r in grid],
            'x': level['worldX'] // GRID, 'y': (bottom - level['worldY'] - level['pxHei']) // GRID,
            'title': field_value(level['fieldInstances'], 'title', level['identifier']),
            'theme': field_value(level['fieldInstances'], 'theme', 'cave'), 'fields': fields,
            'settings': {name: field_value(level['fieldInstances'], name) for name in ROOM_FIELDS
                         if field_value(level['fieldInstances'], name)}}


# ----------------------------------------------------------------------------------------------
# Text
# ----------------------------------------------------------------------------------------------

def read_text_world(list_path):
    folder = os.path.dirname(list_path)
    rooms, theme, extra, settings = [], 'cave', [], []
    for line in open(list_path):
        words = line.split('#', 1)[0].split() if not line.lstrip().startswith('#') else []
        if not words:
            continue
        if words[0] == 'theme':
            theme = words[1]
        elif words[0] == 'entity':
            extra.append(words[1:])
        elif words[0] == 'room':
            settings.append(words[1:])
        else:
            name = words[0][:-4] if words[0].endswith('.txt') else words[0]
            rows = open(os.path.join(folder, name + '.txt')).read().rstrip('\n').split('\n')
            rooms.append({'name': name, 'rows': rows, 'x': int(words[1]), 'y': int(words[2]),
                          'title': ' '.join(words[3:]), 'theme': theme, 'fields': {}, 'settings': {}})
    by_name = {r['name']: r for r in rooms}
    for room, *pairs in settings:
        by_name[room]['settings'].update(dict(p.split('=', 1) for p in pairs))
    for room, x, y, *pairs in extra:
        by_name[room]['fields'][(int(x), int(y))] = dict(p.split('=', 1) for p in pairs)
    return rooms


def write_text_world(rooms, folder):
    os.makedirs(folder, exist_ok=True)
    lines = ['# Rooms exported from the LDtk project by tools/rooms.py. Edit, then import them back.']
    theme = None
    for room in rooms:
        if room['theme'] != theme:
            theme = room['theme']
            lines.append(f'theme {theme}')
        lines.append(f"{room['name']:<12} {room['x']:>5} {room['y']:>5}  {room['title']}")
        open(os.path.join(folder, room['name'] + '.txt'), 'w').write('\n'.join(room['rows']) + '\n')
    for room in rooms:
        if room.get('settings'):
            lines.append(f"room {room['name']} " + ' '.join(f'{k}={v}' for k, v in room['settings'].items()))
    for room in rooms:
        for (x, y), values in sorted(room['fields'].items(), key=lambda kv: (kv[0][1], kv[0][0])):
            lines.append(f"entity {room['name']} {x} {y} " + ' '.join(f'{k}={v}' for k, v in values.items()))
    open(os.path.join(folder, 'rooms.txt'), 'w').write('\n'.join(lines) + '\n')


# ----------------------------------------------------------------------------------------------

def load_project(path):
    return json.load(open(path)) if os.path.exists(path) else new_project()


def to_json(value, indent=0, row_length=None):
    """JSON laid out like LDtk writes it: tabs, short arrays on one line, the tile grid one room
    row per line, so changing a tile changes one line of a diff."""
    tabs = '\t' * indent
    if isinstance(value, dict):
        if not value:
            return '{}'
        items = []
        for k, v in value.items():
            row = value.get('__cWid') if k == 'intGridCsv' else None
            items.append(f'{tabs}\t{json.dumps(k)}: {to_json(v, indent + 1, row)}')
        return '{\n' + ',\n'.join(items) + f'\n{tabs}}}'
    if isinstance(value, list):
        if not value:
            return '[]'
        if row_length:
            rows = [','.join(str(v) for v in value[i:i + row_length]) for i in range(0, len(value), row_length)]
            return '[\n' + ',\n'.join(f'{tabs}\t{r}' for r in rows) + f'\n{tabs}]'
        if all(not isinstance(v, (dict, list)) for v in value):
            return '[' + ','.join(json.dumps(v) for v in value) + ']'
        return '[\n' + ',\n'.join(f'{tabs}\t{to_json(v, indent + 1)}' for v in value) + f'\n{tabs}]'
    return json.dumps(value)


def save_project(project, path):
    with open(path, 'w') as f:
        f.write(to_json(project) + '\n')


def import_rooms(rooms, project):
    ensure_definitions(project)
    old = {level['identifier']: level for level in project['levels']}
    top = max(r['y'] + len(r['rows']) for r in rooms)
    levels = []
    for i, room in enumerate(rooms):
        world_y = (top - room['y'] - len(room['rows'])) * GRID
        levels.append(room_to_ldtk(room, old.get(room['name']), i, world_y))
    project['levels'] = levels
    return project


def export_rooms(project):
    bottom = max(level['worldY'] + level['pxHei'] for level in project['levels'])
    return [ldtk_to_room(level, bottom) for level in project['levels']]


if __name__ == '__main__':
    if len(sys.argv) != 4 or sys.argv[1] not in ('import', 'export'):
        sys.exit(__doc__)
    if sys.argv[1] == 'export':
        write_text_world(export_rooms(load_project(sys.argv[2])), sys.argv[3])
    else:
        save_project(import_rooms(read_text_world(sys.argv[2]), load_project(sys.argv[3])), sys.argv[3])
