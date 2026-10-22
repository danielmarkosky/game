# Music

Every song is a text file here, made into sound by the game when it starts
([src/Synth.cpp](../../src/Synth.cpp)): nothing is recorded. A room plays the song named like its
theme (`cave.txt` in the cave theme), and the title screen plays `title.txt`.

```
tempo 84                 # beats a minute
lead 0.17 triangle       # the melody: volume, and sine, triangle, square, pulse or saw
arp 0.06 up              # the chord's notes one after another: up, down, updown or off
pad 0.14                 # the chord held softly through the bar
bass 0.2 x...x...        # the chord's root, on the steps marked x
drums 0.1 x...x... ....x... ..x...x.   # kick, snare and hat rhythms (optional)
echo 0.3                 # how much echoes back, 0 for none

chords F C Dm Bb | F C Bb C       # one a bar
melody A4 - C5 - F5 - - - | ...   # 8 steps a bar: a note, - to hold it, . for silence
```

- A bar has 8 steps, two to a beat. `|` is only for reading.
- Chords: a root (`C`, `F#`, `Bb`...) then nothing (major), `m`, `7`, `m7`, `maj7`, `sus2`,
  `sus4`, `dim` or `add9`.
- Notes: a letter, maybe `#` or `b`, and an octave: `A4` is the A above middle C.
- The melody has to be as long as the chords: `game_tests` checks every song reads.
- Volumes add up: keep the total under about 1.
- The song loops: the echo at the end carries over into the start.
