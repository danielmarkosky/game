# GAME

To bedzie najlepsza gra na swiecie mowie wam uwierzcie mi bedzie super ekstra jej

Zgadzam się to będzie najlepsza gra zawierająca wszystkie gatunki na świecie


## Zasady

Biorąc pod uwagę że jesteśmy biedni, nie stać nas na github pro. Z tego powodu zamiast normalnie ustawić Rulset, ustalamy zasady tutaj:
- Zmiany robimy na osobnych branchach. Żeby wrzucić na maina trzeba zrobić pull request i mieć 2 apruwy.
- Szanujemy repo, nie wrzucamy śmieci.
- Obowiązkiem wszystkich zaangażowanych jest rozumienie wszystkiego co tutaj sie dzieje.

Kto sie nie stosuje ten zgniła skarpeta.

## Build

```sh
conan install . --build=missing     # SFML, GoogleTest and nlohmann_json
cmake --preset conan-release
cmake --build --preset conan-release
./build/Release/game          # the game
./build/Release/game_tests    # the tests
```

## Controls

| Input | Action |
|-------|--------|
| A / D (or arrows) | Run |
| Space | Jump (hold for a higher jump). On the rope: jump off it |
| Left mouse or Shift | Shoot the rope at the crosshair, hold to keep it, release to let go |
| W / S (on the rope) | Reel in / let out |
| S (on a platform) | Drop through it |
| R | Retry: back to the last checkpoint (counts as a death) |
| M / Tab | The map (also in the pause menu). Arrows or drag to move it, Z or the mouse wheel to zoom |
| W / Up / E (next to someone) | Talk. Space, Enter or a click for the next line, Esc to stop |
| W / Up / E (at a bench) | Sit down and save. Move to get up; M while sitting to travel |
| Esc / P, or click the pause button in the top right | Pause |
| Arrows or W/S, Enter / Space, or the mouse | Use the menus (left / right change a setting) |

At first the rope only sticks to the metal hook blocks. Find the climbing pick, early in The Cave,
and it sticks to rock too. The long rope, at the end of the garden, makes it reach half as far
again: 150 px instead of 100. The crosshair shows a white dot where the rope will stick, and a dim red
one where it would bounce off rock without the pick; no dot means nothing is in range, or the
rope is cooling down. After letting go it can be shot again after 0.3 s, after jumping off it
after 0.55 s.
The rope wraps around corners, so swinging past an edge pivots you around it.

The game also pauses by itself when its window loses focus.

## Levels

There are twelve rooms in one connected world: walk out through a gap in a room's wall (or drop down
a hole in its floor) and the camera pans over into the room on the other side, like in Celeste. You
can always go back the way you came.

You start at the bottom of The Cave and climb The Well to the Crossroads, a hub. From there one way
goes east through the Hook Garden and Curtains, a dead end where the long rope lies. The other goes
up through the Tall Hall, whose crystal walls give nothing to hold on to: its one hook is too high
for a short rope. With the long rope it's the way on, to The Summit, Stepping Stones, The Chimney,
Pendulums, Backtrack and finally Daylight, whose way out is the end of the game.

Coming into a room, the first place you land is your checkpoint, until you reach a flag.

There are people down here too. Wren, an old climber, is right by the start of The Cave, and Pip,
a mapmaker, camps in the Crossroads. A bubble
over someone's head means you're close enough to talk; what they say changes as you get further,
and the save remembers what you've talked about. Their lines are text files in
`assets/dialogue/`, see [its README](assets/dialogue/README.md).

Most rooms hide two gems, 22 in all: high under ceilings, beside spikes, low in a pit you can only
touch with a swing, or tucked into the walls. One, in an alcove in The Cave's ceiling, is in sight
right from the start but can't be reached until you have the pick, so you'll have to come back for
it. Gems you've found stay behind as faint ghosts. The count shows for a moment when you find one,
and while paused; the map shows each room's.

The map shows every room you've been in, each in its own colours, with you blinking on it. Spikes
are red, platforms brown, hook blocks steel, flags green, and anything still to pick up glints gold. Hover a room to see its name.

Every two levels the scenery changes: a purple cave, an overgrown garden, a snowy summit under the moon,
a glowing ember chimney, and finally daylight. The new scenery comes into view with the room.

Benches, like in Hollow Knight, are where the game is saved: sit on one and it glows, and choosing
your slot again later puts you back on it. There's one at the start of The Cave, in the Crossroads,
at the foot of The Summit and in Pendulums. Sitting on a bench you've been to before, press M to
travel: the map shows every bench you've sat on, and you pick one to go to. Dying still only puts
you back where you came into the room, or at the last flag, like in Celeste. What you pick up, the
gems, the map and who you've talked to are saved as soon as they happen; only where you are waits
for a bench.

PLAY on the title screen shows three save slots, each with where it'll carry on, its play time and
its gems (a `*` once it's been finished). An empty one starts a new game. ERASE A SLOT, then a slot,
asks before erasing it. Options has the window size, fullscreen, screen shake, the timer (play time
and deaths, in the corner) and the player's skin: ten to choose from, previewed running under the
menu. Everything is saved in `~/.celeste_like_game/`: `settings.txt`, and `slot1.txt` to
`slot3.txt`. (An older `save.txt` becomes the settings and slot 1 the first time.)

The rooms are made in [LDtk](https://ldtk.io), a free level editor: they're all in
`assets/world.ldtk`. See [assets/levels/README.md](assets/levels/README.md) to change them or make
your own, in LDtk or as text.

For trying rooms out: `./build/Release/game --room tall_hall` starts straight in that room (any
room's identifier), and `./build/Release/game some_room.txt` plays a single text room. Both skip the
title screen, give every ability and don't save anything.

## Art

All the pixel art is drawn as text in [tools/make_art.py](tools/make_art.py): each sprite is a grid
of characters, one per pixel, with a palette saying which character is which colour. Edit a grid
and run `python3 tools/make_art.py` to regenerate the PNGs in `assets/gfx/`. No drawing program needed.

Each theme is a folder in `assets/gfx/themes/`: `tiles.png` and three background layers (`sky.png`,
`far.png`, `near.png`, back to front). To add one, write a `make_<name>()` in the script (the existing
ones are a few lines each: colours, a few decoration grids and the background shapes), add it to
`THEMES`, and add its name to the `Theme` enum in the LDtk project (and to `THEMES` in
`tools/rooms.py`).

Things to pick up are in `ITEMS`, drawn into `items.png`.

Player skins are drawn once, standing; the script makes the other poses (breathing, running, jumping,
falling, swinging) from that and outlines them. To add a skin, add it to `SKINS` in the script and its
name to `Player::SKINS` in [src/Player.h](src/Player.h), in the same order.
