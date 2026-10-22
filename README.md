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

## Tether

A climber falls into a crevasse at the foot of a mountain, following the trail of an older sister,
Ash, who set off for the top three winters ago and never came back. The only way out is up.

*Tether* (a working title: it's `TITLE` in [src/Game.cpp](src/Game.cpp)) is a 2D platformer about a
rope, in a connected world like Hollow Knight's and with Celeste's quick, precise rooms: a pixel
art world of seventeen rooms, abilities that open up the places you've already been, people to talk
to, and an ending that depends on what you tell them.

## Controls

| Input | Action |
|-------|--------|
| A / D (or arrows) | Run |
| Space | Jump (hold for a higher jump). On the rope: jump off it, a full jump. With the feather, again in the air |
| Left mouse or Shift | Shoot the rope at the crosshair, hold to keep it, release to let go |
| W / S (on the rope) | Reel in / let out |
| S (on a platform) | Drop through it |
| W / Up / E | Talk to someone, sit on a bench, pull a lever: a bubble shows when you're close enough |
| Up / Down, Enter / Space or the mouse (talking) | Choose what to say, next line. Esc stops |
| M / Tab | The map (also in the pause menu). Arrows or drag to move it, Z or the mouse wheel to zoom. On a bench: travel |
| R | Retry: back to the last checkpoint (counts as a death) |
| Esc / P, or click the pause button in the top right | Pause |
| Arrows or W/S, Enter / Space, or the mouse | Use the menus (left / right change a setting) |

These are the usual keys: every one but Esc and Enter can be changed in OPTIONS, CONTROLS.

**With a gamepad** (Xbox or PlayStation; others are taken to be laid out like an Xbox pad): the
left stick or d-pad runs, reels and chooses; A / cross jumps and picks in menus; X / square shoots
the rope; B / circle talks, sits and pulls levers, and goes back in menus; Y / triangle is the map;
Back / Share is retry; Start / Options pauses. The rope goes where the stick points, to whatever it
would catch nearest that way (with the stick let go, up and ahead), and the crosshair shows where.
The buttons can be changed in CONTROLS too. The pad that was pressed last is the one listened to.

## Abilities

- **The rope** reaches 100 px from you and at first only sticks to the metal hook blocks. The
  crosshair shows a white dot where it'll stick, and a dim red one where it would bounce off. After
  letting go it can be shot again after 0.3 s, after jumping off it after 0.55 s. It wraps around
  corners, so swinging past an edge pivots you around it. Nothing sticks to slick, wind-polished
  stone.
- **The climbing pick**, early in The Cave: the rope sticks to rock too.
- **The long rope**, at the end of the garden: the rope reaches half as far again, 150 px.
- **The updraft feather**, in the ruins: one more jump in the air, until you land or catch the rope.

## The world

Rooms join up like in Celeste: walk out through a gap in a room's wall (or drop down a hole in its
floor) and the camera pans over into the room on the other side. You can always go back the way you
came, except down the Drop.

You start at the bottom of The Cave, find the pick, and climb The Well to the Crossroads, a hub.
West are the ruins: the Sunken Gate, Mushroom Hollow and the Archive, where the feather is. East is
the Hook Garden and Curtains, a dead end where the long rope lies. Up is the Tall Hall, whose crystal
walls give nothing to hold on to, its one hook too high for a short rope. Beyond it are the
mountains: The Summit (where a lever opens the Drop, a shortcut straight back down to the
Crossroads), Stepping Stones, The Chimney, Pendulums and Backtrack. Then the Windy Ledge, slick stone
over a long fall that only the feather gets you across, and Daylight, whose way out at the top is
the end of the game.

Each part of the world has its own look and its own music: the cave, the garden, the ruins, the
snowy mountains, the ember chimney and the daylight. Coming into a room, the first place you land
is your checkpoint, until you reach a flag.

**People.** Wren by the start of The Cave, Pip the mapmaker in the Crossroads, Moss in the Archive,
Juniper at The Summit's camp, and at the very top, Ash. A bubble over someone means you're close
enough to talk. Some conversations let you choose what to say, and what you choose, like what you've
found, changes what people say later, and how the story ends. Their lines are text files in
`assets/dialogue/`, see [its README](assets/dialogue/README.md).

**Gems.** 26 of them, two in most rooms: high under ceilings, beside spikes, low in a pit you can only
touch with a swing, tucked into the walls. Some are in sight long before you can reach them. Gems
you've found stay behind as faint ghosts; the count shows when you find one and while paused, and
the map shows each room's.

**The map** shows every room you've been in, in its own colours, with you blinking on it: spikes
red, platforms brown, hook blocks steel, flags green, what's still to pick up glinting gold, and the
benches you've sat on.

**Benches**, like in Hollow Knight, are where the game is saved: sit on one and it glows, and choosing
your slot again puts you back on it. Sitting on a bench, M opens the map to travel to any other bench
you've sat on. Dying only puts you back where you came into the room, or at the last flag, like in
Celeste. What you pick up, the gems, the map, the gates you've opened and what you've talked about are
saved as soon as they happen; only where you are waits for a bench.

## Menus and saving

PLAY on the title screen shows three save slots, each with where it'll carry on, its play time and
its gems (a `*` once it's been finished). An empty one starts a new game. ERASE A SLOT, then a slot,
asks before erasing it. Options has the window size, fullscreen, screen shake, the timer (play time
and deaths, in the corner), the player's skin (ten to choose from, previewed running under the menu)
and the music and sound volumes, and CONTROLS: choose an action and press its new key or button.
Everything is saved in `~/.celeste_like_game/`: `settings.txt`, and `slot1.txt` to `slot3.txt`. Each
file is written next to the old one and then swapped in, so a crash while saving can't leave half a
save.

## Making it

The rooms are made in [LDtk](https://ldtk.io), a free level editor: they're all in
`assets/world.ldtk`. See [assets/levels/README.md](assets/levels/README.md) to change them or make
your own, in LDtk or as text.

For trying rooms out: `./build/Release/game --room tall_hall` starts straight in that room (any
room's identifier), and `./build/Release/game some_room.txt` plays a single text room. Both skip the
title screen, give every ability and don't save anything.

**Checking the world can be played.** `python3 tools/check_world.py` sends the level bot
([tools/levelbot.cpp](tools/levelbot.cpp), built with the game) through every room both ways, through
every gate with and without its ability, and to every gem, and says if anything's changed. It takes a
few minutes. When you change a room, add its routes to the list at the top of the script.
`./build/Release/levelbot <room> --goal R` tries one room and prints how it got through, or a map of
where it got stuck.

**Tests.** `game_tests` checks the rules (physics, rope, rooms joining up, saves, dialogue, music),
and plays the game without a window: the slots, benches, travel, talking, levers and the ending. It
also leaves a picture of every theme in `build/screenshots/`.

## Art

All the pixel art is drawn as text in [tools/make_art.py](tools/make_art.py): each sprite is a grid
of characters, one per pixel, with a palette saying which character is which colour. Edit a grid
and run `python3 tools/make_art.py` to regenerate the PNGs in `assets/gfx/`. No drawing program needed.

Each theme is a folder in `assets/gfx/themes/`: `tiles.png` and three background layers (`sky.png`,
`far.png`, `near.png`, back to front). To add one, write a `make_<name>()` in the script (the existing
ones are a few lines each: colours, a few decoration grids and the background shapes), add it to
`THEMES`, add its name to the `Theme` enum in the LDtk project (and to `THEMES` in `tools/rooms.py`),
and give it a song. What drifts in its air and how dark it is are in
[src/Ambience.cpp](src/Ambience.cpp).

Things to pick up are in `ITEMS`, drawn into `items.png`; the font for conversations is `FONT`.

Player skins are drawn once, standing; the script makes the other poses (breathing, running, jumping,
falling, swinging, sitting) from that and outlines them. To add a skin, add it to `SKINS` in the script
and its name to `Player::SKINS` and `Player::LOOKS` in [src/Player.h](src/Player.h), in the same order.
The people you meet are drawn the same way, in `NPCS`, after the skins.

## Sound

Nothing is recorded: [src/Synth.cpp](src/Synth.cpp) makes every sound when the game starts. The sound
effects are a table in [src/Audio.cpp](src/Audio.cpp), and the songs are text files in
`assets/music/`: a chord a bar and a melody over it, see [its README](assets/music/README.md).
