# Level format

A level is a plain text file. Each character is one 8x8 tile, and the first line is the top of
the level. Lines don't need to be the same length.

| Char | Tile |
|------|------|
| `#`  | Solid rock (green tile) |
| `%`  | Metal hook block. Solid like `#`, but drawn so it stands out as something to hook |
| `-`  | One-way platform: jump up through it, stand on top, hold down to drop through |
| `^`  | Metal spikes, kill on touch |
| `x`  | Crystal spikes, same as `^` but pink |
| `S`  | Start position (required, exactly one) |
| `C`  | Checkpoint. Put it in the empty tile right above the ground |
| `G`  | Goal flag. Touching it finishes the level |
| anything else | Empty |

Spikes point away from the rock they're next to (rock below: up, rock above: down, rock on a
side: sideways), and only that pointy half of the tile hurts. Grass, edges and decorations are
picked automatically from the neighbouring tiles.

To add a level to the game, add a line to `levels.txt`: the file name, then the title shown when
it starts.

Things to keep in mind when designing:

- A running jump goes about **4 tiles up** and **6 tiles across**. Gaps of 3–5 tiles are jumps;
  anything wider needs the rope.
- The rope reaches **100 px (12.5 tiles)** from the player's centre and only sticks to solid
  rock (`#`, `%`), not to platforms or hazards.
- The rope wraps around outside corners of rock (and slides along flat walls), so a rope that
  swings past a pit's lip or a block's edge will catch on it. Platforms and hazards don't catch it.
- A swing under a ceiling 9–10 tiles above the floor comfortably crosses 8–10 tiles.
- Keep the walls closed: falling more than 8 tiles below the level kills the player, but walking
  out of a gap in a side wall just lets them wander off.

Play a single level with `./game path/to/level.txt`. With no argument the game plays every level
in `levels.txt`.
