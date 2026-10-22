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
conan install . --build=missing
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
| Esc / P, or click the pause button in the top right | Pause |
| Arrows or W/S, Enter / Space, or the mouse | Use the menus (left / right change a setting) |

At first the rope only sticks to the metal hook blocks. Find the climbing pick, early in The Cave,
and it sticks to rock too. The crosshair shows a white dot where the rope will stick, and a dim red
one where it would bounce off rock without the pick; no dot means nothing is in range, or the
rope is cooling down. After letting go it can be shot again after 0.3 s, after jumping off it
after 0.55 s.
The rope wraps around corners, so swinging past an edge pivots you around it.

The game also pauses by itself when its window loses focus.

## Levels

There are ten: The Cave, The Well, Hook Garden, Curtains, The Summit, Stepping Stones, The Chimney,
Pendulums, Backtrack and Daylight. They're rooms of one connected world: walk out through a gap in a
room's wall (or drop down a hole in its floor) and the camera pans over into the room on the other
side, like in Celeste. You can always go back the way you came. The way out of Daylight, the last
room, is the end of the game.

Coming into a room, the first place you land is your checkpoint, until you reach a flag.

Every room hides two gems, 20 in all: high under ceilings, beside spikes, low in a pit you can only
touch with a swing, or tucked into the walls. One, in an alcove in The Cave's ceiling, is in sight
right from the start but can't be reached until you have the pick, so you'll have to come back for
it. Gems you've found stay behind as faint ghosts. The count shows for a moment when you find one,
and while paused; the level select and the map show each room's.

The map shows every room you've been in, each in its own colours, with you blinking on it. Spikes
are red, platforms brown, hook blocks steel, flags green, and anything still to pick up glints gold. Hover a room to see its name.

Every two levels the scenery changes: a purple cave, an overgrown garden, a snowy summit under the moon,
a glowing ember chimney, and finally daylight. The new scenery comes into view with the room.

The title screen has Continue (from the last room you were in), New Game (from the start, without
the pick or any gems, and with an empty map), Levels (start from any room, with whatever lies in the rooms before
it, and your best time and fewest deaths on each: from going in to going on into the next one) and Options (window size, fullscreen, screen shake, timer, and the player's skin: ten to choose from,
previewed running under the menu). Progress, the rooms
on the map and the settings are saved in `~/.celeste_like_game/save.txt`.

Levels are text files in `assets/levels/`, listed in `assets/levels/levels.txt`; see
[assets/levels/README.md](assets/levels/README.md) to make your own. `./build/Release/game some_level.txt`
plays a single level straight away, without the title screen and without saving results.

## Art

All the pixel art is drawn as text in [tools/make_art.py](tools/make_art.py): each sprite is a grid
of characters, one per pixel, with a palette saying which character is which colour. Edit a grid
and run `python3 tools/make_art.py` to regenerate the PNGs in `assets/gfx/`. No drawing program needed.

Each theme is a folder in `assets/gfx/themes/`: `tiles.png` and three background layers (`sky.png`,
`far.png`, `near.png`, back to front). To add one, write a `make_<name>()` in the script (the existing
ones are a few lines each: colours, a few decoration grids and the background shapes), add it to
`THEMES`, and use it with a `theme` line in `assets/levels/levels.txt`.

Things to pick up are in `ITEMS`, drawn into `items.png`.

Player skins are drawn once, standing; the script makes the other poses (breathing, running, jumping,
falling, swinging) from that and outlines them. To add a skin, add it to `SKINS` in the script and its
name to `Player::SKINS` in [src/Player.h](src/Player.h), in the same order.
