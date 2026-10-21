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
| R | Restart the level |
| Esc / P, or click the pause button in the top right | Pause |
| Arrows or W/S, Enter / Space, or the mouse | Use the menus (left / right change a setting) |

The crosshair shows a white dot where the rope will stick; no dot means nothing is in range, or the
rope is cooling down. After letting go it can be shot again after 0.3 s, after jumping off it
after 0.55 s.
The rope wraps around corners, so swinging past an edge pivots you around it.

The game also pauses by itself when its window loses focus.

## Levels

There are ten, played in order: The Cave, The Well, Hook Garden, Curtains, The Summit, Stepping Stones,
The Chimney, Pendulums, Backtrack and Daylight. Like the screens in Celeste, they're joined up: each one
has a gap in its left, right or top edge, and walking out through it slides the screen over to the next
level. Falling out of the bottom still kills you.

The title screen has Continue (from the last level you reached), Levels (pick any level, with your best
time and fewest deaths on each) and Options (window size, fullscreen, screen shake, timer, and the player's skin: ten to choose from,
previewed running under the menu). Progress and
settings are saved in `~/.celeste_like_game/save.txt`.

Levels are text files in `assets/levels/`, listed in `assets/levels/levels.txt`; see
[assets/levels/README.md](assets/levels/README.md) to make your own. `./build/Release/game some_level.txt`
plays a single level straight away, without the title screen and without saving results.

## Art

All the pixel art is drawn as text in [tools/make_art.py](tools/make_art.py): each sprite is a grid
of characters, one per pixel, with a palette saying which character is which colour. Edit a grid
and run `python3 tools/make_art.py` to regenerate the PNGs in `assets/gfx/`. No drawing program needed.

Player skins are drawn once, standing; the script makes the other poses (breathing, running, jumping,
falling, swinging) from that and outlines them. To add a skin, add it to `SKINS` in the script and its
name to `Player::SKINS` in [src/Player.h](src/Player.h), in the same order.
