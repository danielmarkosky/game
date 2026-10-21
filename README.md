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
| Left mouse | Shoot the rope at the crosshair, hold to keep it, release to let go |
| W / S (on the rope) | Reel in / let out |
| S (on a platform) | Drop through it |
| Space / Enter (level complete) | Next level |
| R | Restart the level |
| Esc | Quit |

The crosshair shows a white dot where the rope will stick; no dot means nothing is in range.
The rope wraps around corners, so swinging past an edge pivots you around it.

## Levels

There are five, played in order: The Cave, The Well, Hook Garden, Curtains and The Summit.
Levels are text files in `assets/levels/`, listed in `assets/levels/levels.txt`; see
[assets/levels/README.md](assets/levels/README.md) to make your own. `./build/Release/game some_level.txt`
plays a single level.

## Art

All the pixel art is drawn as text in [tools/make_art.py](tools/make_art.py): each sprite is a grid
of characters, one per pixel, with a palette saying which character is which colour. Edit a grid
and run `python3 tools/make_art.py` to regenerate the PNGs in `assets/gfx/`. No drawing program needed.
