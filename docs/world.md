# The world of Tether

The plan for the whole world: its regions, how they join, what each one teaches, where everything
is. [`tools/make_world.py`](../tools/make_world.py) builds it, and
[`tools/check_world.py`](../tools/check_world.py) checks every route in it with the level bot. Change
this first when the plan changes.

## How rooms are made

- **One idea a room.** A room teaches one thing, or tests one thing it's already taught. A new
  mechanic first appears where failing is safe, then over a fall, then mixed with what came before.
- **Small.** Most rooms are one or two screens. Big rooms are for places, not for challenges.
- **Readable.** Hook blocks are where the rope is meant to go. Slick stone is where it isn't.
  Danger is red, pink or sharp.
- **Both ways.** Every room can be walked back through, unless it's a one-way shortcut on purpose.
- **Fair.** Sized to what `levelbot --measure` says the moves reach, checked by the bot with exactly
  the abilities you'd have when you get there.
- **Worth going back.** Every ability opens something in a place already seen: a gem, a shortcut,
  a challenge room.

## The regions

| Region | Theme, music | Its idea | Who's there | What you find |
|---|---|---|---|---|
| **The Crevasse** | cave | Walking, jumping, the rope thrown standing | (nobody: Ash's cold camp) | **Ash's rope** |
| **The Hollows** | cave | Climbing up with the rope, ledge by ledge | **Wren** by the bench | a way up |
| **The Crossroads** | garden | The hub: everything starts here | **Pip** the mapmaker, **Tuck** the trader (the shop) | outfits, for gems |
| **The Sunken Ruins** (west) | ruins | Dark rooms lit by lanterns the rope lights; levers the rope pulls from afar | **Moss** in the Archive | **Ash's chalk bag** (climbing) |
| **The Hook Garden** (east) | garden | Hook rings moving on tracks; timing a throw | **Bramble** the gardener: a race | **the long rope** |
| **The Summit** (up) | snow | Long-rope climbs: the Tall Hall | **Juniper** at the high camp | the Drop, a shortcut home |
| **The Frost Caves** (off the Summit) | ice | Ice that slides, icicles that fall when you pass | **Pell**, an ice fisher | **the throwing glove** |
| **The Ember Chimney** (on from the Summit) | ember | Heat that lifts you; blocks that crumble; a race upward | **Cinder** the smith: a race | **the updraft feather** |
| **The Sky** (above it all) | sky | Wind that pushes your swing; clouds that fade under you | **Ash**, at the signal fire | the way out: the end |

## How they join

```
                                                    THE SKY ── Daylight (Ash, the end)
                                                       ▲ (feather)
   FROST CAVES ◄──(chalk)── THE SUMMIT ──(glove)──► EMBER CHIMNEY
                                 ▲     │ the Drop (one way, a lever)
                     (long rope) │     ▼
   SUNKEN RUINS ◄──────────── THE CROSSROADS ──── The Market ──► HOOK GARDEN
                                       ▲
                    The Cave ── The Well (The Deep below)
                       ▲
   THE CREVASSE (where a new game starts)
```

The rooms, region by region (their identifiers in the LDtk project):

- **The Crevasse**: crevasse_wake, crevasse_steps, crevasse_gaps, crevasse_camp, crevasse_dips, crevasse_chasm
- **The Hollows**: level1 (The Cave), level2 (The Well), the_deep
- **The Crossroads**: crossroads, market
- **The Sunken Ruins**: ruins_gate, lever_works, the_vault (up a chimney from the Lever Works), ruins_hollow, dark_stair, ruins_archive
- **The Hook Garden**: level3 (Hook Garden), garden_rings, greenhouse, thorn_walk (above it), trellis, level4 (Curtains)
- **The Summit**: tall_hall, level5 (The Summit), the_drop
- **The Frost Caves**: icefall, frozen_lake, frost_camp, hall_of_icicles (above it), icicle_gallery, frozen_camp
- **The Ember Chimney**: level6 (Stepping Stones), forge, level7 (The Chimney), level8 (Pendulums), crumbling_halls, furnace (below them), the_updraft
- **The Sky**: level9 (Backtrack), windy_ledge, cloud_steps, gusts, last_ledge, eye_of_storm (above it), level10 (Daylight)

**Gates.** The Tall Hall needs the long rope. Into the Frost Caves you climb a wall: the chalk bag.
Out of the Summit towards the Ember Chimney is a hook too high to throw to from the ground: the
glove. The Sky's gaps need the feather.

**Order.** The rope, then the long rope (east) and the chalk bag (west) in either order, then the
glove (Frost Caves), then the feather (Ember Chimney), then the Sky. On the way west, the Lever
Works' gate only opens to a rope tugging its lever through a slit: the first place that's needed.

**Shortcuts**: the Drop, from the Summit straight down to the Crossroads, opened by a lever at the
top. (A shortcut from the ruins to the Cave and a slide from the garden into the Well were planned;
they'd have needed odd shapes of room to line up on the map, and the ruins and garden are close to
the hub anyway.)

**Benches**: The Cave, the Crossroads, the Market, the Archive, the Greenhouse, the Summit, the Frost Camp, the
Forge, Pendulums, the Last Ledge. Travel between any you've sat on.

## Quests

Kept in the pause menu's QUESTS list. Their state is story flags; the rewards are outfits and gems.

| Quest | Given by | What | Reward |
|---|---|---|---|
| **The Last Page** | Moss | A page torn from Moss's oldest book, blown away to the Frost Caves | the Scholar outfit |
| **Through the Garden** | Bramble | A race: from the Greenhouse to Curtains, before the time runs out | the Gardener outfit |
| **Wren's Lantern** | Wren | Wren's old lantern, dropped down The Deep years ago | 5 gems |
| **Lost Lure** | Pell | Pell's lucky lure, carried off to the Ember Chimney | the Fisher outfit |
| **Up the Chimney** | Cinder | A race up the Ember Chimney | the Smith outfit |
| **Ash's notes** | (Ash) | Ash left a note in six places on her way up: read them all, and she gives you her scarf | the Green Scarf outfit |

## The shop

Tuck sells outfits for gems at the Market, next to the Crossroads. The classic look is yours from the
start; the rest are bought or won. Gems are found in every room worth exploring, one or two each, and
more in the challenge rooms. What you've bought shows in OPTIONS, SKIN.

## Challenge rooms

Off the main path, one a region, hard, each with a cache of gems: The Deep (Hollows, below the
Well), The Vault (Ruins, up a chimney only hands climb), the Thorn Walk (Garden, above the
Greenhouse), the Hall of Icicles (Frost Caves, above the camp), the Furnace (Ember, below the
Crumbling Halls), the Eye of the Storm (Sky, above the Last Ledge).

## Ash's notes

One in each region she passed through, where she rested. Together they tell her climb, and why she
stayed at the top.
