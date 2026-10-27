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
./build/Release/editor        # the room editor (see assets/levels/README.md)
./build/Release/game_tests    # the tests
```

## Tether

You come to floating in a hidden lake, in a little cave full of magic, under the waterfall you
fell from, and you don't remember who you are. A road mender gives you a rope with a hook on the
end; by the campfire at the end of the road, Fen tells you why the air is full of drifting lights:
the stars are coming loose. The Lantern on the mountain that held them up has gone dark, and the
old mine line up to it won't run until four beacons burn again, one at the end of each way out of
the camp. Along the way you find your dog, waiting for you at the bottom of the Hollows, and,
behind the waterfall, in the still place between the stars where the falls begin, your Gran's
stopwatch. By the time the cart climbs the mountain you know whose granddaughter you are, and
what you were carrying up the line the night you fell.

*Tether* (a working title: it's `TITLE` in [src/GameShared.h](src/GameShared.h)) is a 2D platformer about a
rope, in a connected world like Hollow Knight's and with Celeste's quick, precise rooms: a pixel
art world, abilities that open up the places you've already been, secrets that wait for them,
people to talk to, and a dog. See [The world](#the-world).

## Controls

| Input | Action |
|-------|--------|
| A / D (or arrows) | Run |
| Space | Jump (hold for a higher jump). On the rope: let go of it. On a wall: leap off it. With the feather, again in the air |
| Left mouse or Shift | Throw the rope at the crosshair, hold to keep it, release to let go |
| Towards a wall, in the air (with the chalk bag) | Grab it. W / S climb, jump leaps off |
| W / S (on the rope) | Reel in / let out |
| A / D, S, Space (in water) | Swim, dive, leap out at the surface. Under water you hold your breath: the blue bar over your head. Come up before it runs out |
| Arrows / WASD (where nothing falls) | Only touching rock: clamber along it. Out in the open nothing moves you but the rope: hook something and W pulls you along the rope; held, it swings you round, and let go, you drift on |
| Space (in a mine cart) | Jump the cart. That's all: it goes where the rails go |
| S (on a bounce mushroom) | Stand on it instead of bouncing |
| S (on a platform) | Drop through it |
| W / Up / E | Talk to someone, sit on a bench, pull a lever: a bubble shows when you're close enough |
| E (with the knot) | Hooked and standing (or holding a wall): tie the rope off where you are. Again: untie it |
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

- **The rope** is thrown: it flies out with the rope paying out behind it, and catches or falls.
  Let go of it (or jump) and it falls too. A loose rope lies on the ground and drapes over edges,
  and it's reeled back in hand over hand, sliding the way it lies: it takes as long as the rope is
  long. It can only be thrown again once it's back in your hand, so a short throw is ready again
  sooner than a long miss. Where you fly after letting go is down to how you swung and
  when you let go. It wraps around corners, so swinging past an edge pivots you around it. The
  crosshair shows a white dot where it'll stick, and a dim red one where it would bounce off.
  It bites rock and the metal hook blocks; nothing sticks to slick, wind-polished stone.
- **The rope and hook**, from Odo the road mender, outside the lake cave: at first it's only thrown standing (or
  holding on to a wall), swung round over your head first, and reaches 64 px.
- **The long rope**: it reaches 104 px.
- **The chalk bag**: push into a wall in the air to grab it, climb up and down, leap off it. Your
  grip lasts a few seconds (the bar over your head), and comes back on the ground. Not slick stone.
- **The throwing glove**: the rope can be thrown in the air too, straight away, reaching a little
  less far there (85%): there's nothing to push off.
- **The updraft feather**: one more jump in the air, until you land or catch the rope.
- **Gran's stopwatch**: times runs of a route, from its green post to its chequered flag, and keeps
  your best (RECORDS, in the pause menu).
- **The knot**, at the end of the Quarry: hooked in rock, and standing or holding on
  to a wall, E ties the rope off where you are, and it stays strung from the hook round its corners
  to the knot: a line to hang from. Fall or jump onto it and you grab it (holding up, reach up for
  it); along it hand over hand; where it's steep you slide down it like a zipline, or climb it
  holding up it; jump to let go. E again unties it and it comes back to your hand. It burns in
  fire, and comes down if what it's hooked in crumbles.

The rope comes from Odo; the long rope and the chalk bag are at the ends of the two routes open from
the start, the glove and the feather at the ends of the two they open, the knot at the bottom of the
Quarry under the Hollows, and the stopwatch behind the
waterfall. A test room has them all.

**Outfits.** Mags keeps a stall by the campfire: clothes, for the gems you've found. The diving suit
lets you hold your breath four times as long (there's somewhere you'll need that), and the miner's
hat carries a lamp in dark places. The rest just look nice. They're in
[assets/outfits.txt](assets/outfits.txt).

**Pip.** Your dog, once you've found him: he follows you everywhere, and now and then points his nose
at the nearest gem still to find in the room and barks. On the map, a paw print marks every room
with gems left in it.

The numbers are all in [assets/physics.txt](assets/physics.txt).

## The world

Rooms join up like in Celeste: walk out through a gap in a room's wall (or drop down a hole in its
floor) and the camera eases over into the room on the other side, without stopping you. Places run
into each other: between two that look different there's a room that's some of each (the Cave
Mouth, the Sinkhole, the Cliff Path, the Snowy Pass), and nearing a way into somewhere that looks different, its scenery and its dark come
in, half and half in the doorway, and the room you've left goes on showing a while past it. And
it's all the one night: the sky over the camp is the sky the Sky Road climbs into. You can always go back the way you
came.

You come to floating in **the Hidden Lake**, swim ashore and walk out through **the Cave Mouth**
onto **the Old Road**, where Odo gives you the rope; past **the Gully**, his practice ground, the road
ends at **the Campfire**, a clearing at the foot of the cliffs: Fen by the fire, a stewpot on the
embers, Mags at her stall, a bench under a string of lanterns. The road goes on east; the sinkhole
is boarded over (drop through, down **the Sinkhole** to the Hollows); and up the cliff, **the Cliff
Path**, where the other ways go off.

- **The Hook Garden**, east: swings over spikes, bounce mushrooms, hook rings that carry you. At its
  end, the long rope. Open from the start.
- **The Hollows**, down the sinkhole: the old workings, where a block of stone has to be dragged onto
  a weight plate to open the gate (the rope pulls it where the rope bends, so lower yourself off the
  shelf it's on and reel it in from below), and a brazier the rope mustn't swing into; a hollow so dark you light lamps with the rope
  to see, and the den at the bottom: the chalk bag, and Pip. Open from the start.
- **The Frost Caves**, off the Cliff Path and through **the Snowy Pass**: a chasm only the long rope crosses, icicles over ice, a
  frozen waterfall to swing up. At its end, the throwing glove.
- **The Sky Road**, at the top of the Cliff Path: a wall only the chalk bag climbs, clouds that
  fade under you, updrafts in the wind. At its end, the feather.
- **The Old Line**, off the Cliff Path to the station: once all four beacons burn, the cart takes you up
  the mountain, over the broken bridge (a jump and a second jump), to the Lantern: up to it with
  everything you've got, and past it, the end of the game.

Under the Hollows, through a trapdoor of planks by the foot of the shaft, **the Quarry**: a long drop
down the Rigging, a shaft with nothing to climb or swing from; the Kiln, where flues of fire breathe
on and off and you swing past them while they're cold; and the Cutting, where a block of stone
dragged through a cold flue is the step up to the riggers' knot. The way back up the Rigging is
climbing lines tied off with it (or the bench).

And secrets. Behind the lake's waterfall the rock isn't there: through it, **the Still**, where
there's no gravity at all, only black and stars, and the stopwatch: nothing moves you there but the
rope, and the walls are crystal (come back with the rope); up inside it, a climb for the
long rope to **the Hidden Spring**. Down a crack in the lake bed, a tunnel longer than any breath
lasts, to **the Drowned Grotto** (the diving suit). And along every route, gems that wait for an
ability you'll only have later, and rock here and there that isn't there either.

The rooms are made in [`tools/world/`](tools/world/), a file each part of the world, and
[docs/world.md](docs/world.md) is the plan, room by room.

Every room is small, one idea each, and checked by the level bot with exactly the abilities you'd
have when you get there.

Each part of the world has its own look and its own music: the grotto of the Hidden Lake and the
glade under the aurora, the cave, the garden, the frost caves, the sky, the black of the Still and
the mountain the Old Line climbs (and ready for more, the ruins, the snowy mountains, the ember
chimney and the daylight). Coming into a room, the first place you land is your checkpoint, until
you reach a flag.

**People.** Odo mending the Old Road (and later, the line), Fen by the Campfire, Mags at her stall. A bubble over someone means you're
close enough to talk. Some conversations let you choose what to say, and what you choose, like what
you've found, changes what people say later. Their lines are text files in `assets/dialogue/`, see
[its README](assets/dialogue/README.md).

**Gems** are to collect, and to spend at Mags's stall: the ones you've found stay behind as faint
ghosts, the count shows when you find one and while paused, and the map shows each room's.

**The map** shows every room you've been in, in its own colours, with you blinking on it: spikes
red, platforms brown, hook blocks steel, flags green, what's still to pick up glinting gold, people,
and the benches you've sat on. Zoomed out, each region's name is written over it.

**Sounds** warn you before the danger: stone cracks before it crumbles, an icicle rattles before it
falls. You hear the waterfall, the wind where it blows and the roar of heat near an updraft, and
bubbles when you're under water.

**Benches**, like in Hollow Knight, are where the game is saved: sit on one and it glows, and choosing
your slot again puts you back on it. Sitting on a bench, M opens the map to travel to any other bench
you've sat on. Dying only puts you back where you came into the room, or at the last flag, like in
Celeste. What you pick up, the gems, the map, the gates you've opened and what you've talked about are
saved as soon as they happen; only where you are waits for a bench.

**The end** is past the Lantern at the top of the Old Line: the end screen tells how it all turned
out, then how long it took, how many times you died, your gems and your records.

## Menus and saving

PLAY on the title screen shows three save slots, each with where it'll carry on, its play time and
its gems (a `*` once it's been finished). An empty one starts a new game. ERASE A SLOT, then a slot,
asks before erasing it. Options has the window size, fullscreen, screen shake, the timer (play time
and deaths, in the corner) and the music and sound volumes, and CONTROLS: choose an action and press
its new key or button.
Everything is saved in `~/.celeste_like_game/`: `settings.txt`, and `slot1.txt` to `slot3.txt`. Each
file is written next to the old one and then swapped in, so a crash while saving can't leave half a
save.

## Making it

The rooms are designed in `tools/world/`, a file of Python per part of the world, a few lines each room, and
`python3 tools/make_world.py` joins them up and writes them into `assets/world.ldtk`, the
[LDtk](https://ldtk.io) project the game loads. It won't write over changes made to the project in
LDtk since it last wrote it. See [assets/levels/README.md](assets/levels/README.md) for how rooms work,
the tiles and the things in them, and how far each move reaches.

Rooms can also be laid out with the mouse, in the game's own art: `./build/Release/editor`
(see [the room editor](assets/levels/README.md#the-room-editor)) opens any room, the ones made in
Python too, or makes a new one. Ctrl+S saves the room and puts it in the world, F5 plays it right
there in the editor as it is, saved or not (F5 again and you're back editing where you got to;
Shift+F5 plays from the mouse), and Tab shows the world map, where rooms are dragged into place.

For trying rooms out: `./build/Release/game --room gully` starts straight in that room (any
room's identifier), and `./build/Release/game some_room.txt` plays a single text room. Both skip the
title screen, give every ability and don't save anything.

**How it moves.** Every number behind the feel (run speed, how quickly you speed up and slow down,
gravity, the jump, how hard you can push a swing, how far a fling carries) is in
[assets/physics.txt](assets/physics.txt), each with what it does. Change one and press **F5** while
playing: no compiling. `./build/Release/game assets/levels/playground.txt` is a room for trying it
out: steps 1 to 4 tiles high, gaps 3 to 7 wide, a pit to swing across, a long flat floor to see how
far a fling carries, a slick wall to get over and a tall one to climb. A test room starts with every
ability, and the keys **1 to 7** switch them off and on: the rope, the long rope, the chalk bag, the
throwing hand, the feather, the stopwatch and the knot. `./build/Release/game assets/levels/tie_off.txt`
is a room for trying the knot: a pit to string a line across, a wall to climb a line up, and a
zipline down to a pillar over spikes.

**Checking the world can be played.** `python3 tools/check_world.py` sends the level bot
([tools/levelbot.cpp](tools/levelbot.cpp), built with the game) through every room both ways, through
every gate with and without its ability, and to every gem, and says if anything's changed. When you change a room, add its routes to the list at the top of the script.
`./build/Release/levelbot <room> --goal R` tries one room and prints how it got through, or a map of
where it got stuck. `./build/Release/levelbot --measure` prints how far a jump, a throw, a climb and a
swing reach with the physics as they are, for designing rooms (see the table in
[assets/levels/README.md](assets/levels/README.md)).

**Tests.** `game_tests` checks the rules (physics, rope, swimming and holding your breath, rooms
joining up, saves, dialogue, music), and plays the game without a window: the slots, benches,
travel, talking, levers, drowning and the ending. It also leaves pictures of the themes in use in
`build/screenshots/`.

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

The abilities, gems and levers are in `ITEMS`, drawn into `items.png`; the font for conversations is `FONT`.
The scenery is in `PROPS`, drawn into `props.png`, with `props.txt` saying where each one is, how
many frames it has, whether it hangs and what light it gives: the game reads that. Add a prop there,
then place it in a room with `room.prop(x, y, 'name')` in its region's file in `tools/world/`.

The heroine (`HEROINE`) is drawn once, standing, and once with her hair streaming out behind her;
the script makes the other poses (breathing, running, jumping, falling, swinging, sitting, climbing)
from that and outlines them. The people you meet are drawn the same way, in `NPCS`, after her; then
Pip, frame by frame (`PIP_FRAMES`); then the heroine in her other outfits (`OUTFITS`: her drawing
with its colours and rows changed). Each look's name is in `Player::LOOKS` in
[src/Player.h](src/Player.h), in the same order.

## Sound

Nothing is recorded: [src/Synth.cpp](src/Synth.cpp) makes every sound when the game starts. The sound
effects are a table in [src/Audio.cpp](src/Audio.cpp), and the songs are text files in
`assets/music/`: a chord a bar and a melody over it, see [its README](assets/music/README.md).
