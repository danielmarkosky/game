#pragma once

#include <SFML/Config.hpp>
#include <string>
#include <vector>

// Makes the game's sounds and music from descriptions, like tools/make_art.py makes its pictures:
// nothing is recorded. Mono, 16 bit.
namespace synth
{
    constexpr unsigned RATE = 32000; // samples a second

    enum class Wave { Sine, Triangle, Square, Pulse, Saw, Noise };

    // A sound effect: a tone sliding from one pitch to another, maybe stepping through an
    // arpeggio on the way, shaped by a quick fade in and a fade out.
    struct Effect
    {
        Wave wave = Wave::Square;
        float startHz = 440.f;
        float endHz = 440.f;
        float duration = 0.1f;   // seconds
        float attack = 0.005f;   // seconds to full volume
        float release = 0.05f;   // seconds of fading out at the end
        float volume = 0.3f;     // 0 to 1
        float noise = 0.f;       // how much noise to mix in, 0 to 1
        std::vector<int> arpeggio; // semitones above the pitch, played one after the other
    };
    std::vector<sf::Int16> render(const Effect& effect);

    // A piece of music, read from a text file (see assets/music/README.md): a chord a bar, a
    // melody over them, and the parts the chords are played as.
    struct Song
    {
        float tempo = 90.f;     // beats a minute
        int steps = 8;          // steps a bar, two to a beat
        std::vector<std::vector<int>> chords; // per bar: MIDI notes, root first
        std::vector<int> melody;              // per step: a MIDI note, -1 rest, -2 hold the last
        // Each part: its volume (0 is off), and for some the wave or rhythm.
        float lead = 0.2f, arp = 0.1f, bass = 0.25f, pad = 0.12f, drums = 0.f;
        Wave leadWave = Wave::Triangle;
        std::string bassRhythm = "x...x...";   // a step per character: x plays
        std::string kick = "x...x...", snare = "....x...", hat = "..x...x.";
        std::string arpStyle = "up";           // up, down, updown or off
        float echo = 0.25f;                    // feedback of the echo, 0 for none

        float seconds() const;
    };
    // Throws std::runtime_error, saying where, if the text isn't a song.
    Song parseSong(const std::string& text);
    // Renders the song so that it loops seamlessly: the echo carries round from the end to the start.
    std::vector<sf::Int16> render(const Song& song);

    // "A4" -> 69 (MIDI), "C#5", "Bb3". -1 if it isn't a note.
    int noteNumber(const std::string& name);
    // "Am" -> its notes in the third octave, root first: Am, F, C7, Dm7, Gmaj7, Esus4, Bdim...
    std::vector<int> chordNotes(const std::string& name);
}
