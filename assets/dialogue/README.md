# Conversations

Each character's lines are a text file here, named in the `dialogue` field of their `Npc` entity
in the LDtk project (`wren` is `wren.txt`). Their `look` field is how they look: one of the player
skins, or one of the people drawn for the game (`wren`, `pip`, `moss`, `juniper`, `ash`; see `NPCS`
in tools/make_art.py).

```
# Lines starting with # are notes, not said.

---
Wren: Well, well! Somebody came up out of the crevasse.
Wren: Let's have a look at that rope.
? It's my sister's. Ash.                  -> sister
? I found it in an old camp down there.   -> found
= sister
You: Her name's Ash.
Wren: Ash. She camped down there for days, learning that rope.
set told_wren
-> rope
= found
Wren: That was her camp.
-> rope
= rope
Wren: You throw it like you've never held one.
And this line is Wren's too: no name means the same speaker as the line before.

--- told_wren !long_rope
Wren: Up the well, climber.
```

- **Lines.** Every line is one box of text, typed out; a long one is split over several boxes.
  `You` is the player, with their skin as the face. The name is a capitalised word or two before
  `: `.
- **Conversations.** `---` starts a new one. Talking to a character plays the first one you haven't
  had yet that the facts allow; once you've had them all, the last allowed one again. So the first
  is what they say when you meet, and the last is what they keep saying.
- **Facts.** After `---` can come facts that must hold for that conversation, all of them: an
  ability (`rope`, `long_rope`, `wall_climb`, `air_throw`, `double_jump`) or a flag set in a
  conversation. With `!`, one that mustn't hold yet (`--- told_wren !long_rope`).
- **Choices.** `? what you say -> label` lines next to each other are a choice: the player picks one,
  and the conversation goes on from `= label`. Reaching the next `= label` line ends the branch;
  `-> label` jumps to another, and `-> end` ends the conversation.
- **Flags.** `set name` remembers that something happened, for this save. Other conversations can
  depend on it, and so does the ending.
- **The ending.** `endings.txt` is read the same way: the end screen shows the lines of the first
  conversation whose facts hold. Keep the last one without any, for when none fit.
- Stick to plain ASCII: letters, digits and the usual punctuation. That's what the font has.
  `game_tests` checks it, that every character's file is there, and that every label a choice or a
  jump goes to exists.
