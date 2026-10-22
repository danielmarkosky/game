# Conversations

Each character's lines are a text file here, named in the `dialogue` field of their `Npc` entity
in the LDtk project (`wren` is `wren.txt`). Their `look` field is which of the player skins they
wear (`classic`, `kid`, `climber`, `explorer`, `astronaut`, `frog`, `knight`, `fox`, `robot` or
`mushroom`).

```
# Lines starting with # are notes, not said.

--- !climbing_pick
Wren: Oh! A climber, down here?
You: I'm trying to get out.
Wren: Mind the spikes.
And this line is Wren's too: no name means the same speaker as the line before.

--- climbing_pick
Wren: Is that my old pick?
```

- Every line is one box of text, typed out; a long one is split over several boxes. `You` is the
  player, with their skin as the face. The name is a capitalised word or two before `: `.
- `---` starts a new conversation. Talking to a character plays the first one you haven't had
  yet; once you've had them all, the last one again. So the first is what they say when you meet,
  and the last is what they keep saying.
- After `---` can come an ability the player must have for it (`--- climbing_pick`), or, with
  `!`, one they mustn't have yet (`--- !climbing_pick`). Conversations the player's abilities
  don't allow are skipped, so a character's lines can change once you find something.
- Stick to plain ASCII: letters, digits and the usual punctuation. That's what the font has.
  `game_tests` checks it, and that every character's file is there.
