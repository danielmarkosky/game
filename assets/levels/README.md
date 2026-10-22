# Making rooms

The world is one LDtk project: [`assets/world.ldtk`](../world.ldtk). Open it in
[LDtk](https://ldtk.io), the free level editor, to change rooms or add new ones. Every LDtk level
is a room of the game, and the world view (`F2`, or the zoomed-out view) is the game's map.

## In LDtk

Each room has two layers, 8 px to a tile:

| Tiles layer value | Tile |
|-------------------|------|
| `rock` | Solid rock (how it looks depends on the theme) |
| `hook_block` | Metal hook block. Solid like rock, but drawn so it stands out as something to hook |
| `platform` | One-way platform: jump up through it, stand on top, hold down to drop through |
| `spikes` | Metal spikes, kill on touch |
| `crystal` | Crystal spikes, same as spikes but pink. The rope goes through them to whatever's behind |
| `gate` | Solid, until a lever in the same room is pulled: then every gate in the room opens for good |
| `slick` | Slick, wind-polished stone: solid, but the rope slides off it. For places that mustn't be swung across |

| Entity | |
|--------|---|
| `Start` | Where the game starts, in the first room, and where `--room` starts a room (required, exactly one) |
| `Checkpoint` | A flag. Put it in the empty tile right above the ground |
| `Goal` | Touching it finishes the game (optional) |
| `Pickup` | Gives an ability: its `ability` field says which. `climbing_pick` makes the rope stick to rock too, not just to hook blocks; `long_rope` makes it reach 150 px instead of 100 |
| `Gem` | A gem, to collect. It can float in the air: touching it is enough |
| `Lever` | Pulling it (W next to it) opens the room's gates, and the save remembers |
| `Bench` | Two tiles wide: sitting on it saves, and from it you can travel to any other bench sat on. Both tiles need ground under them |
| `Npc` | Someone to talk to, standing on the bottom of the tile. `dialogue` names their file in [`assets/dialogue/`](../dialogue/README.md), `look` the player skin they wear |

Each room also has two fields: its `title`, shown the first time you go in, and its `theme`:
`cave`, `garden`, `snow`, `ember` or `daylight` (the folders in `assets/gfx/themes/`). A theme only
changes the looks, never how a tile behaves. Grass, edges and decorations are picked automatically
from the neighbouring tiles, and spikes point away from the rock they're next to (rock below: up,
rock above: down, rock on a side: sideways). Only that pointy half of the tile hurts.

The first room in the project is where a new game starts, and the last one has the way out at the
end of the game.

## The world

Like in Celeste, you go from room to room by walking off the screen: through a gap in a room's
outside wall into the room on the other side, and back again the same way. Rooms can join on any
side, the bottom too (drop down a hole into the room below). A gap in the bottom with no room under
it is a pit, and falling out of it kills. The only gap that may lead nowhere else is in the last
room: that's the end of the game.

Put rooms right next to each other in the world view, so that the gaps in the two walls line up
tile for tile. `game_tests` checks that they do, that no rooms overlap, and that every room can be
reached (it doesn't check the jumps, so play it).

## As text

[`tools/rooms.py`](../../tools/rooms.py) turns the project into one text file per room and back,
for big edits with a script or a text editor:

```sh
python3 tools/rooms.py export assets/world.ldtk rooms/     # rooms/rooms.txt and a file per room
python3 tools/rooms.py import rooms/rooms.txt assets/world.ldtk
```

Each character of a text room is one tile, and the first line is the top of the room:

| Char | | Char | |
|------|---|------|---|
| `#` | rock | `S` | Start |
| `%` | hook block | `C` | Checkpoint |
| `-` | platform | `G` | Goal |
| `^` | spikes | `P` | Pickup (`climbing_pick` unless rooms.txt says otherwise) |
| `x` | crystal spikes | `*` | Gem |
| `=` | gate | `L` | Lever |
| `o` | slick stone | | |
| `.` | empty | `N` | Npc (its fields in rooms.txt) |
| | | `B` | Bench |

`rooms.txt` lists the rooms in order: a `theme <name>` line sets the theme of the rooms below it,
then each room is its file name, its position on the map (in tiles, of its bottom-left tile, x to
the right and y up) and its title. `entity <room> <column> <row> name=value ...` lines give an
entity's fields.

`./build/Release/game some_room.txt` plays a single text room on its own, and
`./build/Release/game --room <identifier>` starts the world in that room. Neither has a title
screen, both give every ability and neither saves (in a text room on its own, any gap in it ends
the game).

## Designing

- The numbers below come from [`assets/physics.txt`](../physics.txt): change those and they change.
  `./build/Release/levelbot --measure` measures them again. In tiles:

  | Move | Reach |
  |------|-------|
  | Jump | 3.8 up, standing or running; 4.8 across at a run. Gaps of 3–4 are jumps, wider ones need more |
  | Double jump (the feather), again at the top | 6.8 up, 7.9 across |
  | Rope throw | 8 from the ground, 6 in the air (with the throwing hand); the long rope 13 and 9.8 |
  | Climbing (the chalk bag) | about 10 up a wall from the ground before the grip runs out |
  | One swing along the floor, ceiling of hooks 5 / 7 / 9 up | 12.8 / 14.8 / 9.6 across; with the long rope 15.4 / 21.5 / 20.4, and 19.2 under 11 |

- The rope only sticks to solid rock and hook blocks, not to platforms or hazards. Until the player
  has the climbing pick it only sticks to hook blocks, so anything before the pick has to be done
  without rock to swing from. Until the throwing hand it's only thrown standing (or holding a wall).
- With the pick, a rock wall can be climbed: hook it high, reel in, let go, hook it higher. Jumping
  on the rope only lets go, with no jump of its own: height and distance come from the swing. To
  make something really out of reach, use slick stone (no rope, no hands), or line the walls with
  crystal (the rope goes through crystal to the rock behind, but hanging from there drags you into
  the spikes), like the Tall Hall.
- The Windy Ledge's gaps are 7 and 8: they need the feather.
- `python3 tools/check_world.py` checks every room can be got through, both ways, and every gate
  holds without its ability: add your room's routes to it.
- The rope wraps around outside corners of rock (and slides along flat walls), so a rope that
  swings past a pit's lip or a block's edge will catch on it. Platforms and hazards don't catch it.
- Rooms can be walked through both ways, so make sure the way back works too.
- For a way up into the room above, put something to stand on 2-3 tiles below the gap. Going up
  into a room gives the player a lift of a full jump, so put a ledge or a one-way platform within
  about 3 tiles above the bottom of the room above, to land on.
- Coming into a room, the first place the player stands becomes their checkpoint, until they reach
  a flag.
