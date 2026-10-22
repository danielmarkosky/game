# Level format

A level is a plain text file. Each character is one 8x8 tile, and the first line is the top of
the level. Lines don't need to be the same length.

| Char | Tile |
|------|------|
| `#`  | Solid rock (how it looks depends on the theme) |
| `%`  | Metal hook block. Solid like `#`, but drawn so it stands out as something to hook |
| `-`  | One-way platform: jump up through it, stand on top, hold down to drop through |
| `^`  | Metal spikes, kill on touch |
| `x`  | Crystal spikes, same as `^` but pink |
| `S`  | Start position (required, exactly one) |
| `C`  | Checkpoint. Put it in the empty tile right above the ground |
| `G`  | Goal flag (optional). Touching it finishes the game |
| `P`  | The climbing pick: after picking it up the rope sticks to rock too, not just to `%` |
| `*`  | A gem, to collect. It can float in the air: touching it is enough |
| anything else | Empty |

**The world.** Every level is a room on one big map. Like in Celeste, you go from room to room by
walking off the screen: through a gap in a room's outside wall into the room on the other side,
and back again the same way. Rooms can join on any side, the bottom too (drop down a hole into
the room below). A gap in the bottom with no room under it is a pit, and falling out of it kills.
The only gap that may lead nowhere else is in the last room in `levels.txt`: that's the end of the
game.

Spikes point away from the rock they're next to (rock below: up, rock above: down, rock on a
side: sideways), and only that pointy half of the tile hurts. Grass, edges and decorations are
picked automatically from the neighbouring tiles.

To add a level to the game, add a line to `levels.txt`: the file name, where it goes on the map,
then the title shown the first time you go in. The position is in tiles, of the level's bottom-left
tile, with x to the right and y up. Put it right next to the room it joins, so that the gaps in
the two walls line up tile for tile. `game_tests` checks that they do, that no rooms overlap, and
that every room can be reached (it doesn't check the jumps, so play it).

**Themes.** A `theme <name>` line in `levels.txt` sets the scenery of the levels below it, until
the next `theme` line: `cave`, `garden`, `snow`, `ember` or `daylight` (the folders in
`assets/gfx/themes/`). A theme only changes the looks, never how a tile behaves. Levels before any
`theme` line, and levels played on their own from the command line, are in the cave.

Things to keep in mind when designing:

- A running jump goes about **4 tiles up** and **6 tiles across**. Gaps of 3–5 tiles are jumps;
  anything wider needs the rope.
- The rope reaches **100 px (12.5 tiles)** from the player's centre and only sticks to solid
  rock (`#`, `%`), not to platforms or hazards. Until the player has the climbing pick it only
  sticks to hook blocks (`%`), so anything before the pick has to be done without rock to swing
  from. Starting a room from the level select gives the abilities picked up in the rooms above
  it in `levels.txt`.
- The rope wraps around outside corners of rock (and slides along flat walls), so a rope that
  swings past a pit's lip or a block's edge will catch on it. Platforms and hazards don't catch it.
- A swing under a ceiling 9–10 tiles above the floor comfortably crosses 8–10 tiles.
- Rooms can be walked through both ways, so make sure the way back works too.
- For a way up into the room above, put something to stand on 2-3 tiles below the gap. Going up
  into a room gives the player a lift of a full jump, so put a ledge or a one-way platform within
  about 3 tiles above the bottom of the room above, to land on.
- Coming into a room, the first place the player stands becomes their checkpoint, until they reach
  a flag.

Play a single level with `./game path/to/level.txt` (any gap in it ends the game). With no
argument the game plays the world in `levels.txt`.
