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
| `G`  | Goal flag (optional). Touching it also finishes the level, with a fade instead of a slide |
| anything else | Empty |

**Exits.** Like in Celeste, a level is finished by walking off the screen: leave a gap in the
outside wall on the left, the right or the top, and going through it slides the screen over to the
next level. The bottom is never an exit, falling out of it kills.

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
- Keep the walls closed except for the exit: any gap in the left, right or top edge that the player
  can reach is a way out to the next level. Falling more than 8 tiles below the level kills.
  `game_tests` checks that every level in `levels.txt` has an exit the start is connected to
  (it doesn't check the jumps, so play it).
- For an exit at the top, put something to stand on 2-3 tiles below the gap, so a jump carries
  the player out.

Play a single level with `./game path/to/level.txt`. With no argument the game plays every level
in `levels.txt`.
