# Conversations

Each character's lines are a text file here, named in the `dialogue` field of their `Npc` entity
in the LDtk project (`odo` is `odo.txt`). Their `look` field is how they look: one of the people
drawn for the game (`odo`, `fen`, `mags`, and `pip` the dog; see `NPCS` in tools/make_art.py and
`Player::LOOKS`). With `pose` `sit` they sit on the ground. With `needs` they're only there once
that fact holds, with `until` only until it does: Odo moves from the road to the Old Station once
you've the long rope, and Pip leaves his den with you once he's yours (`dog`).

```
# Lines starting with # are notes, not said.

---
Odo: Well, well! Somebody came out of the lake cave.
? I fell. From up there, with the water.  -> fell
? Who are you?                            -> who
= fell
Odo: Fell! And not a scratch on you.
set told_odo
-> rope
= who
Odo: Odo. I mend the road.
-> rope
= rope
Odo: Here, take this rope. The hook bites rock.
give ability rope
And this line is Odo's too: no name means the same speaker as the line before.

--- told_odo !long_rope
Odo: Mind the ditch.
```

- **Lines.** Every line is one box of text, typed out; a long one is split over several boxes.
  `You` is the player, with her face. The name is a capitalised word or two before `: `.
- **Conversations.** `---` starts a new one. Talking to a character plays the first one you haven't
  had yet that the facts allow; once you've had them all, the last allowed one again. So the first
  is what they say when you meet, and the last is what they keep saying.
- **Facts.** After `---` can come facts that must hold for that conversation, all of them: an
  ability (`rope`, `long_rope`, `wall_climb`, `air_throw`, `double_jump`) or a flag set in a
  conversation. With `!`, one that mustn't hold yet (`--- told_odo !long_rope`).
- **Choices.** `? what you say -> label` lines next to each other are a choice: the player picks one,
  and the conversation goes on from `= label`. Reaching the next `= label` line ends the branch;
  `-> label` jumps to another, and `-> end` ends the conversation.
- **Flags.** `set name` remembers that something happened, for this save. Other conversations can
  depend on it.
- **Commands.** `give ability rope`: an ability handed over, like picking it up (Odo's rope).
  `open shop`: once the talking's done, Mags's stall opens (outfits for gems, assets/outfits.txt).
- A line's speaker who has a look of their own (`Fen:`) shows their own face.
- Stick to plain ASCII: letters, digits and the usual punctuation. That's what the font has.
  `game_tests` checks it, that every character's file is there, and that every label a choice or a
  jump goes to exists.
