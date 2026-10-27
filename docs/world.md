# The world of Tether

The plan for the world, and where everything is. The rooms are made in
[`tools/world/`](../tools/world/), a file per part of the world (or by hand, in the room editor:
[`tools/world/rooms/`](../assets/levels/README.md#the-room-editor)), and
[`tools/make_world.py`](../tools/make_world.py) joins them and builds the project;
[`tools/check_world.py`](../tools/check_world.py) checks every route in it with the level bot. Change
this first when the plan changes.

## The story

The Lantern on the mountain held the stars up, for as long as anyone remembers. Its keeper, an old
woman hard as rope, tended it; then one night it went dark, and the stars started coming loose: the
lights drifting in the air everywhere tonight are them. The keeper's granddaughter was taking a new
spark up the old mine line to light it again, her dog in the cart with her, when the bridge over the
falls gave way. She fell, over the falls, into the Hidden Lake, and woke with no memory.

That's you. Odo, mending the old road, gives you a rope. Fen, reading the stars by the campfire,
tells you about the Lantern and the line: its cart won't run until the four beacons burn, one at the
end of each way out of the camp. Down in the Hollows you find Pip, who has waited on a ledge since the
night you fell, and remembers you before you remember yourself. Behind the waterfall, in the Still
where the falls begin, your Gran's stopwatch has been waiting too. With all four beacons lit, the
cart climbs the mountain, over the broken bridge where you fell, and you light the Lantern. The stars
go home. (The lines are in [`assets/dialogue/`](../assets/dialogue/), the ending in
`Game::finishGame`.)

## How it's laid out

```
                                                The Sky Road (D) ── The Updraft Cliffs ── The Sky's Edge: the feather
                                                       │
 The Frozen Falls ── The Icicle Hall ── The Frost Caves (C) ── The Snowy Pass ── The Cliff Path ── The Old Station ── The Old Line ── The Broken Bridge ── The Lantern
   the glove                                                                          │           (E, the cart)                                              the end
 Hidden Spring                                                                        │
     │                                                                                │
 The Still ── Where the Falls Begin    The Hidden Lake ── The Cave Mouth ── The Old Road ── The Gully ── The Campfire ── The Hook Garden (A) ── The Mushroom Glade ── The Ring Orchard: the long rope
     (behind the waterfall)                │                                                              │
                                     The Drowned Grotto                                             The Sinkhole
                                                                                                          │
                                                                    The Hollows (B) ── The Old Workings ── The Dark Hollow ── Pip's Den: the chalk bag, and Pip
                                                                              │ (a trapdoor)
                                          The Cutting: the knot ── The Kiln ── The Rigging
```

**The routes.** Two are open from the start, A and B; what's at the end of each opens one of the
other two: the long rope (A) the Frost Caves (C), the chalk bag (B) the Sky Road (D). The glove (C)
and the feather (D) are for the secrets, and with all four the beacons all burn and the Old Line (E)
runs. Each route is three rooms (the Hollows four), its first an ability's gate where it is one, its last a hill with
its ability, its beacon (lit once you've the ability), a bench to travel back from, and the finish
of its timed run (the start is in its first room).

| Route | Way in | Region, theme | Needs | Mechanics | At its end |
|---|---|---|---|---|---|
| **A. The Hook Garden** ([`garden.py`](../tools/world/garden.py)) | East, along the road | The Hook Garden, garden | The rope | Swings over spikes; bounce mushrooms; hook rings that carry you | **The long rope** |
| **B. The Hollows** ([`hollows.py`](../tools/world/hollows.py)) | Down the sinkhole | The Hollows, cave | The rope | A stone dragged onto a weight plate to open a gate (the rope pulls it where the rope bends: over the shelf's edge, from below), and a brazier the rope mustn't swing into; crumbling stone; a dark hollow lit by lamps you light with the rope; climbing out (with what's at the bottom) | **The chalk bag**, and **Pip** up on a ledge (climb to him) |
| **C. The Frost Caves** ([`frost.py`](../tools/world/frost.py)) | Off the Cliff Path, left, through the Snowy Pass | The Frost Caves, ice | The long rope (a chasm only it crosses) | Ice underfoot; icicles that fall as you pass; hook-to-hook up a frozen waterfall | **The throwing glove** |
| **D. The Sky Road** ([`sky.py`](../tools/world/sky.py)) | At the top of the Cliff Path | The Sky, sky | The chalk bag (a wall nothing else gets you up) | Clouds that fade under you; updrafts in the wind | **The updraft feather** |
| **E. The Old Line** ([`line.py`](../tools/world/line.py)) | Off the Cliff Path, right, to the station | The Old Line, mine | All four beacons (the cart won't move till then) | Riding a mine cart: jump the gaps, don't jump under crystals, the broken bridge (jump twice); then up to the Lantern with all four | **The Lantern**, and the way out: the end of the game |

**The Quarry** ([`quarry.py`](../tools/world/quarry.py)), under the Hollows, through a trapdoor of
planks by the foot of the shaft, open from the start (you need the rope), and a way back out that
needs what's at the end of it:

| Room | What it is | Needs |
|---|---|---|
| **The Rigging** | A tall shaft, slick walls and roof, ledges up it with slick faces and a hook block at one corner each. Down: a drop. Up: hook the next ledge's corner, tie the rope off on the one you're on, climb the line; off the top ledge up through the trapdoor | Down: nothing. Up: the knot |
| **The Kiln** (ember) | A low hall with flues: columns of fire floor to roof that burn and go cold in turn. One to walk past; two pits with flues in them and hook blocks between, to swing over while they're cold | The rope |
| **The Cutting** | A slick-faced ledge too high to jump to, with **the knot** and a bench; a flue below it and a block of stone beyond: hook the stone while the flue's cold, reel it to your feet, step up | The rope (the gem under the roof: the long rope) |

**The secrets**, behind and under the Hidden Lake:

| Room | What it is | Needs |
|---|---|---|
| The wall behind the waterfall | Rock that isn't there (`?`): swim into the falls and step into it. Inside, on the left, the way to the Still; and steps up inside the wall to where the long rope reaches the rock over the crack, and the passage at its top | Nothing; the long rope, up |
| **The Still**, **Where the Falls Begin** ([`still.py`](../tools/world/still.py)) | No gravity at all: black, stars, rocks adrift with crystals on them, and crystal walls. Nothing moves you but the rope: hook a rock and reel yourself to it, or hold on drifting past and swing round it. At the end, the rift the falls pour out of, and **Gran's stopwatch**. A pocket of rock that isn't there, with a gem | The rope |
| **The Hidden Spring** ([`secret.py`](../tools/world/secret.py)) | A bench of its own, the stream that becomes the falls, gems | The long rope; the feather for one gem |
| **The Drowned Grotto** ([`drowned.py`](../tools/world/drowned.py)) | Down a crack in the lake bed, a flooded tunnel longer than a breath, to a pocket of air with gems | The diving suit (Mags's stall) |

**Secrets along the routes**, each a gem in sight that waits for something you'll only have later:
the garden's pillar by the way in (the chalk bag, or the long rope), the hollow in the glade's roof
(the feather, or the long rope), the dark hollow's ledge (the long rope), the den's gem under the
roof (climbing to Pip's rock, then the long rope), the slick shelf in the Frost Caves and the shelf
over the Frozen Falls (the feather), the hook blocks over the Sky's Edge (a throw from a jump: the
glove). And here and there, gems behind rock that isn't there.

## The opening

| Room | Region | What happens | Teaches |
|---|---|---|---|
| **The Hidden Lake** (`lake`) | The Hidden Lake | You come to floating in the lake, under the waterfall you fell from (left). Swim to a rock in the middle, then ashore on the right, where the lake bed rises. A bench on the shore. Behind the falls and under the lake, the secrets above | Swimming, diving and coming up for air; walking |
| **The Cave Mouth** (`cave_mouth`) | The Hidden Lake | Out of the grotto: its roof and glowing plants give out partway, and beyond, grass, the moon and the stars. A room that's some of each, so the cave doesn't stop dead at the edge of the night | Walking |
| **The Old Road** (`old_road`) | The Vale | Out of the cave mouth into the night. Odo is mending the road where the bridge washed out, and gives you a rope with a hook. Hook blocks under the cave's lip, over the ditch. Once you've the long rope, he's gone up to the station | Throwing the rope standing, one swing |
| **The Gully** (`gully`) | The Vale | Odo's practice ground: a swing over a ditch; an overhang with hook blocks over a ledge too high to jump, to reel in up to; a long swing over a pool | Swinging, reeling in, letting go at the right time |
| **The Campfire** (`camp`) | The Vale | A clearing at the foot of the cliffs: Fen by the fire, a stewpot, a tent, firewood, a bench under a string of lanterns, Mags at her stall up the bank. The hub: the road east; the sinkhole, boarded over, down; up the cliff (a wooden step, a shelf, boards up its face), the Cliff Path | Jumping up through planks; dropping through them |
| **The Cliff Path** (`cliff_path`) | The Vale | Shelves of rock up both sides of a cliff, a jump apart, the trees thinning out. Off it on the left the Snowy Pass, on the right the Old Station, at the top the Sky Road | Climbing |
| **The Snowy Pass** (`snowy_pass`) | The Frost Caves | The grass giving out under snow; pines, a cairn; the ice caves' mouth at the far end | (A way between: glade to ice) |
| **The Sinkhole** (`sinkhole`) | The Hollows | Under the camp's boards: roots at the top, wet rock lower down; boards up alternate sides to climb back up by | (A way between: glade to cave) |

## How rooms are made

- **One idea a room.** A room teaches one thing, or tests one thing it's already taught. A route's
  mechanic first appears where failing is safe, then over a fall, then mixed with what came before,
  then at its hardest just before the end.
- **Small.** Most rooms are one or two screens. Dying puts you back where you came into the room.
- **Readable.** Hook blocks are where the rope is meant to go. Slick stone (polished, with a sheen)
  is where it isn't. Danger is red, pink or sharp.
- **Forward.** A route's rooms only have to be climbed one way: its last room has a bench to travel
  back from. Going back through one is allowed, but never needed.
- **Fair.** Sized to what `levelbot --measure` says the moves reach, checked by the bot with exactly
  the abilities you'd have when you get there.
- **Secrets say so.** A secret you can't reach yet is in sight: a gem on a shelf too high, a hook too
  high to throw to from the ground, a gem in a hole in the rock. You know to come back, and what
  for. Only the waterfall's keeps quiet: Fen hints at it, once you've the long rope.
