# Making rooms

The world is one LDtk project: [`assets/world.ldtk`](../world.ldtk), which the game loads. Every
LDtk level is a room of the game, and the world view (`F2`, or the zoomed-out view) is the game's
map.

The rooms are designed in [`tools/world/`](../../tools/world/), a file per region: each room drawn
in a few lines of Python, sized to the moves `levelbot --measure` prints.
[`tools/make_world.py`](../../tools/make_world.py) joins them door to door (its `JOINS`) and writes
them all into the LDtk project. That replaces the rooms in it, so change a room in its region's file,
not in [LDtk](https://ldtk.io). If the project was changed in LDtk since the script last wrote it,
the script stops rather than lose that (`--force` writes over it anyway; or stop using the script
for good, and from then on change rooms in LDtk). Either way, `python3 tools/check_world.py`
afterwards.

Or draw a room by hand, in [the room editor](#the-room-editor): those rooms are saved in
`tools/world/rooms/`, and the script puts them in the world with the rest.

## The room editor

`./build/Release/editor` opens the room editor, drawn in the game's own art, with a list of the rooms
to open (or make a new one). `./build/Release/editor <name>` opens that room straight away: its file
`tools/world/rooms/<name>.txt`; or, if there's none, the room of that name made in Python, as it is in
the world (saving it makes its file, which the world uses instead from then on: delete the file to go
back to the Python one); or else a new room of 40 x 23 tiles (a screen), or
`editor <name> <width> <height>` for another size. Pick a tile or a thing on the left and lay the room
out with the mouse:

| | |
|---|---|
| Left button | Paint. Things (Start, a person, a prop...) go one at a time; only one Start |
| Right button | Erase. Erasing part of the outside wall makes a door (not a corner: those stay rock) |
| Shift + drag | A rectangle of it (with the right button: erase one) |
| The handles on the room's four sides | Drag them out or in to make the room bigger or smaller on that side (any size from 3 x 3) |
| Alt (Option) + click | Pick up what's there, with its fields |
| Wheel; middle button or Space + drag, or arrows | Zoom; move the view |
| A tile's or thing's character (`#`, `%`, `S`...) | Pick it |
| Ctrl+Z, Ctrl+Shift+Z | Undo, redo (Cmd on a Mac) |
| Ctrl+S | Save, and rebuild the world (`tools/make_world.py`): then it's in the game |
| F5 | Play the room right here, as it is (saved or not), with every ability (1-6 switch them, R back to the last place you stood). F5 or Esc again: back to editing, looking at where you got to |
| Shift + F5 | The same, from the tile under the mouse |
| F6 | Play the world from it, in the game |
| Ctrl+O | Open another room (or click its name) |
| Tab | The world map (below) |

The panel also sets what new things get (what a pickup gives, what a person says and looks like, which
prop; click a thing again to change it), the room's title, theme, region, size (typed in, or a side
at a time: Shift for 5 tiles), wind and darkness, and,
under DOORS, where each gap in its outside wall leads: to a door as wide in the facing wall of another
room. The rooms are placed on the map so joined doors line up. A room that isn't joined to anything
yet is left out of the world (F5 still plays it; nor does it need a Start yet), and the world needs a door joined to nothing, the
way out at the end of the game: those doors all have to be in one room, which goes last. Then add the room's routes to
`tools/check_world.py`. Saving also runs the game's checks of the world (`game_tests`, `World.*`):
the status bar says which fail and in which room, and the terminal says why.

**The world map** (Tab, or "World map" in the panel) shows every room where it is in the world, and
under it the hand-made rooms that aren't in it yet. Drag a room to move it: it snaps to where one of
its doors meets a door as wide in another room, and when you let go it's joined to the rooms whose
doors meet its own there (and only those), its file is saved and the world built again. Doors that
meet are green, the ways out orange, pits red. It won't let go of a room where it would overlap
another, meet none, cut a room off from the world, or leave doors that lead nowhere in more than one
room (they're the end of the game, so they're all in the last room). A room made in Python gets its
own file when it's moved. Ctrl+Z undoes a move; click a room to edit it.

The file is text (see [`src/RoomFile.h`](../../src/RoomFile.h)): the room's settings, its joins,
the fields of the things in it, then its tiles in the characters of [As text](#as-text).
`./build/Release/game tools/world/rooms/<name>.txt` plays one, and `levelbot` tries one the same way.

## In LDtk

Each room has two layers, 8 px to a tile:

| Tiles layer value | Tile |
|-------------------|------|
| `rock` | Solid rock (how it looks depends on the theme) |
| `hook_block` | Metal hook block. Solid like rock, but drawn so it stands out as something to hook |
| `platform` | One-way platform: jump up through it, stand on top, hold down to drop through |
| `spikes` | Metal spikes, kill on touch |
| `crystal` | Crystal spikes, same as spikes but pink. The rope goes through them to whatever's behind |
| `gate` | Solid, until a lever in the same room is pulled: then every gate in the room opens for good. Or while a stone holds down a weight plate (`Plate`) in the room |
| `slick` | Slick, wind-polished stone: solid, but the rope slides off it. For places that mustn't be swung across |
| `ice` | Solid, and the rope bites it, but you slide on it (slow to start, slower to stop) and hands can't hold it |
| `crumble` | Cracked stone: it gives way half a second after you stand on it, and comes back a moment later. A rope hooked in it comes loose |
| `cloud` | A one-way platform that fades from under you after a moment, and comes back |
| `water` | Not solid: you swim in it, floating with your head out. Left / right swim, down dives, jump at the surface leaps out. A tile deep is enough to swim in. Ground level with its surface can be swum out onto. With your head under, you hold your breath: 8 seconds (`air_time`) and you drown, so an underwater tunnel needs air pockets or a short way through (or the diving suit: four times as long, `diving_air`) |
| `bounce` | A bounce mushroom: solid, and landing on it throws you up far higher than a jump (`bounce_speed`), again and again. Holding down, you stand on it. The rope slides off it, like slick stone |
| `hidden` | A hidden passage: drawn as rock (and the rock round it as if it were rock too), but not there at all: walk straight in. In one, it fades away round you so you can see where you're going. The rope goes through it. In an outside wall it's a door like any gap |
| `rail` | Rails, for mine carts: a one-way platform, like planks |

| Entity | |
|--------|---|
| `Start` | Where the game starts, in the first room, and where `--room` starts a room (required, exactly one) |
| `Checkpoint` | A flag. Put it in the empty tile right above the ground |
| `Goal` | Touching it finishes the game (optional) |
| `Pickup` | Gives an ability: its `ability` field says which: `rope` (the rope and hook), `long_rope`, `wall_climb` (the chalk bag), `air_throw` (the throwing hand), `double_jump` (the updraft feather) or `tie_off` (the knot). Abilities come from these, or from a conversation (`give ability rope`) |
| `Gem` | A gem, to collect. It can float in the air: touching it is enough |
| `Lever` | Pulling it (W next to it, or a rope thrown at it) opens the room's gates, and the save remembers |
| `Lamp` | A lantern hanging from the top of its tile, unlit: a rope thrown at it lights it, for good. For dark rooms |
| `Icicle` | Hangs from the top of its tile; shakes when you go under it, falls, grows back |
| `Updraft` | Heat rising from its tile, two tiles wide, up to the first rock: it lifts you |
| `Ring` | A hook ring sliding to and fro: `to` is where its track ends, "dx,dy" in tiles right and up; `period` the seconds there and back. The rope catches it and goes where it goes |
| `Bench` | Two tiles wide: sitting on it saves, and from it you can travel to any other bench sat on. Both tiles need ground under them |
| `Npc` | Someone to talk to, standing on the bottom of the tile. `dialogue` names their file in [`assets/dialogue/`](../dialogue/README.md), `look` how they look (one of `Player::LOOKS`), `pose` `sit` to sit on the ground (by a fire) instead of standing. `needs`: only there once that fact holds (an ability, or a flag set in a conversation); `until`: only until it does |
| `Cart` | A mine cart on rails: walking into it gets you in, and it rolls off `dir`-wards (`1` right, `-1` left) at `cart_speed` until it reaches a `CartStop`. All you can do in it is jump (and jump again, with the feather); running into a wall wrecks it. `needs`: what has to hold before it moves, by commas (the Old Line's: all four abilities). Dying while riding puts you back in it where it came into the room |
| `CartStop` | The end of the line: a cart riding into it stops, and you get out |
| `Trial` | A post for timing runs with the stopwatch: `route` is which route (one of assets/regions.txt), `mark` `start` or `finish`. Passing a start post starts the clock; that route's finish stops it, and the best time is kept (RECORDS, in the pause menu). Only seen once you have the stopwatch |
| `Stone` | A block of stone, two tiles square (its tile is the bottom-left one): solid, it falls, and the rope hooks into it. Hooked, with your feet on the ground (or hands on a wall), reeling in drags it along at `stone_drag_speed` towards wherever the rope bends first, or towards you if it runs straight: so wrapped round a corner, the rope pulls it another way than towards you, like over a pulley. Held like that, it doesn't fall. It's back where it was when you die or come back into the room |
| `Plate` | A weight plate, two tiles wide (its tile is the left one), standing on the floor: a `Stone` resting on it holds the room's gates open, while it's there. You're too light to |
| `Fire` | Fire, burning in its tile and up into the one above: it burns you, and burns the rope through wherever the rope touches it, thrown, hooked, hanging slack or tied off (the end beyond is lost and the rest comes back). `flue` `1` makes it a column of fire from its tile up to the first rock. `period`: it breathes, burning for half that many seconds and cold for the other half, and flickers for half a second before it lights again; `phase` (0 to 1) is how far through that it starts, so flues can take turns |
| `Prop` | Scenery: its `kind` is a prop in [`assets/gfx/props.txt`](../gfx/props.txt) (a cold fire, a tent, bookshelves, braziers, a signal fire, a beacon...). With `needs`, it's drawn as `<kind>_off` until that fact holds (the beacons: unlit until you've their route's ability). It stands on the bottom of its tile, or hangs from its top (banners, vines, chains, lanterns, icicles). Some glow, some flicker. `light_shaft` is a shaft of light falling from its tile to the ground below, and `waterfall` water pouring from its tile (and the one to its right) down to the first water or rock below, in front of the player, with spray and its own sound. Nothing to touch: it's only looks |

Each room also has two fields: its `title`, shown the first time you go in, and its `theme`:
`cave`, `garden`, `snow`, `ember`, `daylight`, `ruins`, `ice`, `sky`, `grotto`, `glade`, `still` or `mine` (the folders in `assets/gfx/themes/`).
And some settings: `wind`, `dark`, and `space`: `1` for no gravity at all, everything floating
(the Still): nothing you do moves you out in the open, you drift on the way you were going; touching
rock you hold on to it and can clamber along it, never off it. Only the rope moves you: reeled in,
it pulls you along itself, and held, it swings you round where it's hooked. The rope hangs no
slack there either. A theme only
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
| `^` | spikes | `P` | Pickup (`rope` unless rooms.txt says otherwise) |
| `x` | crystal spikes | `*` | Gem |
| `=` | gate | `L` | Lever |
| `o` | slick stone | `Q` | Lamp |
| `~` | ice | `Y` | Icicle |
| `&` | crumbling stone | `U` | Updraft |
| `_` | cloud | `R` | Ring (6 tiles right, 4 s, unless rooms.txt says otherwise) |
| `w` | water | | |
| `.` | empty | `N` | Npc (its fields in rooms.txt) |
| | | `B` | Bench |
| | | `D` | Prop (its `kind` in rooms.txt) |

Rooms can be windy (`wind`, pixels a second each second, + to the right, on anything in the air)
and dark (`dark` 1: nothing to see but what's lit, the player's own little light and lamps).
A room's `region` (one of [`assets/regions.txt`](../regions.txt)) is the name the map writes over
it. rooms.txt gives them as `room <name> wind=-70 dark=1 region=vale`; in LDtk they're the room's
fields.

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
  | Rope throw | 8 from the ground, 6.8 in the air (with the throwing glove); the long rope 13 and 11. So a hook 14 above the floor is out of reach from the ground, but not from the top of a jump |
  | Climbing (the chalk bag) | about 10 up a wall from the ground before the grip runs out |
  | One swing along the floor, ceiling of hooks 5 / 7 / 9 up | 12.8 / 14.8 / 9.6 across; with the long rope 15.4 / 21.5 / 20.4, and 19.2 under 11 |

- The rope sticks to rock and hook blocks, not to platforms, hazards or slick stone. Hook blocks
  catch the eye: use them where the rope is meant to go. Until the throwing hand it's only thrown
  standing (or holding a wall), so early rooms need somewhere to stand within reach of each hook.
- A rock wall can be climbed with the rope: hook it high, reel in, let go, hook it higher. Jumping
  on the rope only lets go, with no jump of its own: height and distance come from the swing. To
  make something really out of reach, use slick stone (no rope, no hands), or line the walls with
  crystal (the rope goes through crystal to the rock behind, but hanging from there drags you into
  the spikes).
- Swimming, your feet float 7 px under the surface, and swimming into a ledge up to 9 px above your
  feet steps you out onto it: so ground whose top is level with the water's surface (or a tile below
  it) can be swum out onto. Anything higher needs a leap out at the surface: about 3 tiles up.
  Water breaks a fall, so a pool under a long swing makes missing it safe; give it a way out.
  Under the surface the air lasts 8 seconds: at a dive's 45 px a second, that's about 20 tiles
  there and back, so keep a dive to a few tiles, or give it somewhere to come up.
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
