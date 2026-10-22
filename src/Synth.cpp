#include "Synth.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace synth
{
    namespace
    {
        constexpr float PI = 3.14159265f;

        float midiToHz(float note)
        {
            return 440.f * std::pow(2.f, (note - 69.f) / 12.f);
        }

        // A small fast random number source, the same every time, for noise.
        struct Noise
        {
            uint32_t state = 0x12345678u;
            float next()
            {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                return static_cast<float>(state) / 2147483648.f - 1.f;
            }
        };

        // One cycle of a wave, `phase` going 0 to 1.
        float wave(Wave w, float phase)
        {
            switch (w) {
                case Wave::Sine: return std::sin(2.f * PI * phase);
                case Wave::Triangle: return 1.f - 4.f * std::abs(phase - 0.5f);
                case Wave::Square: return phase < 0.5f ? 0.6f : -0.6f;
                case Wave::Pulse: return phase < 0.25f ? 0.6f : -0.6f;
                case Wave::Saw: return (2.f * phase - 1.f) * 0.7f;
                case Wave::Noise: return 0.f;
            }
            return 0.f;
        }

        std::vector<sf::Int16> toSamples(const std::vector<float>& mix)
        {
            std::vector<sf::Int16> out(mix.size());
            for (size_t i = 0; i < mix.size(); ++i) {
                // A soft limit: loud passages bend instead of clipping.
                const float v = std::tanh(mix[i]);
                out[i] = static_cast<sf::Int16>(std::lround(v * 32000.f));
            }
            return out;
        }

        // A note played by a voice: a wave at a pitch with an envelope, added into `mix`.
        void addNote(std::vector<float>& mix, size_t start, float seconds, float hz, Wave w, float volume,
                     float attack, float decay, float vibrato = 0.f)
        {
            const size_t length = static_cast<size_t>(seconds * RATE);
            float phase = 0.f;
            for (size_t i = 0; i < length && start + i < mix.size(); ++i) {
                const float t = static_cast<float>(i) / RATE;
                float env = std::min(1.f, t / std::max(attack, 0.001f));
                env *= std::exp(-t / std::max(decay, 0.001f));
                const float fade = std::min(1.f, (seconds - t) / 0.01f); // no click at the end
                const float wobble = vibrato > 0.f && t > 0.15f ? std::sin(2.f * PI * 5.f * t) * vibrato : 0.f;
                phase += hz * std::pow(2.f, wobble / 12.f) / RATE;
                phase -= std::floor(phase);
                mix[start + i] += wave(w, phase) * env * fade * volume;
            }
        }

        void addDrum(std::vector<float>& mix, size_t start, char kind, float volume, Noise& noise)
        {
            const float seconds = kind == 'k' ? 0.18f : kind == 's' ? 0.14f : 0.05f;
            const size_t length = static_cast<size_t>(seconds * RATE);
            float phase = 0.f, held = 0.f;
            for (size_t i = 0; i < length && start + i < mix.size(); ++i) {
                const float t = static_cast<float>(i) / RATE;
                float v = 0.f;
                if (kind == 'k') {
                    // A thump: a sine falling quickly in pitch.
                    phase += (45.f + 90.f * std::exp(-t * 30.f)) / RATE;
                    v = std::sin(2.f * PI * phase) * std::exp(-t * 18.f);
                } else if (kind == 's') {
                    if (i % 3 == 0) held = noise.next();
                    v = held * std::exp(-t * 22.f) * 0.6f;
                } else {
                    v = noise.next() * std::exp(-t * 70.f) * 0.35f;
                }
                mix[start + i] += v * volume;
            }
        }

        std::vector<std::string> words(const std::string& line)
        {
            std::istringstream in(line);
            std::vector<std::string> out;
            for (std::string w; in >> w;) {
                if (w != "|") out.push_back(w);
            }
            return out;
        }

        Wave waveNamed(const std::string& name)
        {
            if (name == "sine") return Wave::Sine;
            if (name == "triangle") return Wave::Triangle;
            if (name == "square") return Wave::Square;
            if (name == "pulse") return Wave::Pulse;
            if (name == "saw") return Wave::Saw;
            throw std::runtime_error("no wave called " + name);
        }
    }

    std::vector<sf::Int16> render(const Effect& e)
    {
        std::vector<float> mix(static_cast<size_t>(e.duration * RATE), 0.f);
        Noise noise;
        float phase = 0.f, held = 0.f, holdPhase = 0.f;
        for (size_t i = 0; i < mix.size(); ++i) {
            const float t = static_cast<float>(i) / RATE;
            const float along = t / e.duration;
            float hz = e.startHz * std::pow(e.endHz / e.startHz, along);
            if (!e.arpeggio.empty()) {
                const size_t step = std::min(e.arpeggio.size() - 1, static_cast<size_t>(along * e.arpeggio.size()));
                hz *= std::pow(2.f, e.arpeggio[step] / 12.f);
            }
            phase += hz / RATE;
            phase -= std::floor(phase);
            // Noise changes value at the tone's pitch, so it has a pitch of its own, like sfxr's.
            holdPhase += hz * 2.f / RATE;
            if (holdPhase >= 1.f) {
                holdPhase -= std::floor(holdPhase);
                held = noise.next();
            }
            const float tone = e.wave == Wave::Noise ? held : wave(e.wave, phase);
            const float v = tone * (1.f - e.noise) + held * e.noise;
            const float env = std::min(1.f, t / std::max(e.attack, 0.0005f)) * std::min(1.f, (e.duration - t) / std::max(e.release, 0.0005f));
            mix[i] = v * env * e.volume;
        }
        return toSamples(mix);
    }

    float Song::seconds() const
    {
        return static_cast<float>(chords.size() * steps) * 30.f / tempo;
    }

    int noteNumber(const std::string& name)
    {
        static const int semitone[] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
        if (name.size() < 2 || name[0] < 'A' || name[0] > 'G') return -1;
        int n = semitone[name[0] - 'A'];
        size_t i = 1;
        if (name[i] == '#') ++n, ++i;
        else if (name[i] == 'b') --n, ++i;
        if (i >= name.size() || !std::isdigit(static_cast<unsigned char>(name[i]))) return -1;
        return n + 12 * (std::stoi(name.substr(i)) + 1);
    }

    std::vector<int> chordNotes(const std::string& name)
    {
        if (name.empty() || name[0] < 'A' || name[0] > 'G') return {};
        size_t i = 1;
        if (i < name.size() && (name[i] == '#' || name[i] == 'b')) ++i;
        const int root = noteNumber(name.substr(0, i) + "3");
        const std::string kind = name.substr(i);
        std::vector<int> steps;
        if (kind == "") steps = {0, 4, 7};
        else if (kind == "m") steps = {0, 3, 7};
        else if (kind == "7") steps = {0, 4, 7, 10};
        else if (kind == "m7") steps = {0, 3, 7, 10};
        else if (kind == "maj7") steps = {0, 4, 7, 11};
        else if (kind == "sus2") steps = {0, 2, 7};
        else if (kind == "sus4") steps = {0, 5, 7};
        else if (kind == "dim") steps = {0, 3, 6};
        else if (kind == "add9") steps = {0, 4, 7, 14};
        else return {};
        std::vector<int> notes;
        for (int s : steps) notes.push_back(root + s);
        return notes;
    }

    Song parseSong(const std::string& text)
    {
        Song song;
        std::istringstream lines(text);
        int lineNumber = 0;
        for (std::string line; std::getline(lines, line);) {
            ++lineNumber;
            // A comment starts with '#' at the start or after a space: in a note, '#' is a sharp.
            if (!line.empty() && line[0] == '#') line.clear();
            if (const size_t hash = line.find(" #"); hash != std::string::npos) line.resize(hash);
            const std::vector<std::string> w = words(line);
            if (w.empty()) continue;
            auto fail = [&](const std::string& why) { throw std::runtime_error("line " + std::to_string(lineNumber) + ": " + why); };
            const std::string& key = w[0];
            try {
                if (key == "tempo") song.tempo = std::stof(w.at(1));
                else if (key == "steps") song.steps = std::stoi(w.at(1));
                else if (key == "lead") song.lead = std::stof(w.at(1)), song.leadWave = w.size() > 2 ? waveNamed(w[2]) : song.leadWave;
                else if (key == "arp") song.arp = std::stof(w.at(1)), song.arpStyle = w.size() > 2 ? w[2] : song.arpStyle;
                else if (key == "bass") song.bass = std::stof(w.at(1)), song.bassRhythm = w.size() > 2 ? w[2] : song.bassRhythm;
                else if (key == "pad") song.pad = std::stof(w.at(1));
                else if (key == "echo") song.echo = std::stof(w.at(1));
                else if (key == "drums") {
                    song.drums = std::stof(w.at(1));
                    if (w.size() > 4) song.kick = w[2], song.snare = w[3], song.hat = w[4];
                } else if (key == "chords") {
                    for (size_t i = 1; i < w.size(); ++i) {
                        auto notes = chordNotes(w[i]);
                        if (notes.empty()) fail("no chord called " + w[i]);
                        song.chords.push_back(notes);
                    }
                } else if (key == "melody") {
                    for (size_t i = 1; i < w.size(); ++i) {
                        if (w[i] == ".") song.melody.push_back(-1);
                        else if (w[i] == "-") song.melody.push_back(-2);
                        else if (int n = noteNumber(w[i]); n >= 0) song.melody.push_back(n);
                        else fail("no note called " + w[i]);
                    }
                } else {
                    fail("don't know " + key);
                }
            } catch (const std::logic_error&) {
                fail("a number is missing or wrong");
            }
        }
        if (song.chords.empty()) throw std::runtime_error("no chords");
        const size_t steps = song.chords.size() * static_cast<size_t>(song.steps);
        if (!song.melody.empty() && song.melody.size() != steps) {
            throw std::runtime_error("the melody has " + std::to_string(song.melody.size()) + " steps, the chords " + std::to_string(steps));
        }
        for (const std::string* rhythm : {&song.bassRhythm, &song.kick, &song.snare, &song.hat}) {
            if (rhythm->size() != static_cast<size_t>(song.steps)) throw std::runtime_error("a rhythm isn't " + std::to_string(song.steps) + " steps long");
        }
        return song;
    }

    std::vector<sf::Int16> render(const Song& song)
    {
        const float step = 30.f / song.tempo; // seconds: two steps a beat
        const size_t loop = static_cast<size_t>(song.seconds() * RATE);
        // Rendered twice over, keeping the second time round: by then the echo and the notes
        // ringing on past the end have wrapped round to the start, so the loop is seamless.
        std::vector<float> mix(loop * 2, 0.f);
        Noise noise;
        auto at = [&](size_t pass, size_t stepIndex) { return pass * loop + static_cast<size_t>(stepIndex * step * RATE); };
        const size_t bars = song.chords.size();
        for (size_t pass = 0; pass < 2; ++pass) {
            for (size_t bar = 0; bar < bars; ++bar) {
                const std::vector<int>& chord = song.chords[bar];
                const size_t first = bar * static_cast<size_t>(song.steps);
                const float barSeconds = step * static_cast<float>(song.steps);
                // Pad: the chord held through the bar, soft, a bit higher.
                if (song.pad > 0.f) {
                    for (int note : chord) addNote(mix, at(pass, first), barSeconds + 0.3f, midiToHz(static_cast<float>(note + 12)), Wave::Triangle, song.pad / chord.size(), 0.25f, 3.f);
                }
                for (int s = 0; s < song.steps; ++s) {
                    const size_t start = at(pass, first + static_cast<size_t>(s));
                    if (song.bass > 0.f && song.bassRhythm[static_cast<size_t>(s)] == 'x') {
                        addNote(mix, start, step * 1.8f, midiToHz(static_cast<float>(chord[0] - 12)), Wave::Triangle, song.bass, 0.005f, 0.4f);
                    }
                    if (song.arp > 0.f && song.arpStyle != "off") {
                        const size_t n = chord.size();
                        size_t i = static_cast<size_t>(s) % n;
                        if (song.arpStyle == "down") i = n - 1 - i;
                        if (song.arpStyle == "updown") i = (static_cast<size_t>(s) / n) % 2 ? n - 1 - i : i;
                        addNote(mix, start, step * 0.9f, midiToHz(static_cast<float>(chord[i] + 24)), Wave::Pulse, song.arp, 0.003f, 0.12f);
                    }
                    if (song.drums > 0.f) {
                        if (song.kick[static_cast<size_t>(s)] == 'x') addDrum(mix, start, 'k', song.drums, noise);
                        if (song.snare[static_cast<size_t>(s)] == 'x') addDrum(mix, start, 's', song.drums, noise);
                        if (song.hat[static_cast<size_t>(s)] == 'x') addDrum(mix, start, 'h', song.drums, noise);
                    }
                }
            }
            // The melody: each note held through the '-'s after it.
            if (song.lead > 0.f) {
                for (size_t s = 0; s < song.melody.size(); ++s) {
                    if (song.melody[s] < 0) continue;
                    size_t length = 1;
                    while (s + length < song.melody.size() && song.melody[s + length] == -2) ++length;
                    addNote(mix, at(pass, s), step * static_cast<float>(length) + 0.05f, midiToHz(static_cast<float>(song.melody[s])), song.leadWave,
                            song.lead, 0.02f, 0.6f + 0.25f * static_cast<float>(length), 0.15f);
                }
            }
        }
        // Echo: a dotted eighth later, quieter each time round.
        if (song.echo > 0.f) {
            const size_t delay = static_cast<size_t>(step * 1.5f * RATE);
            for (size_t i = delay; i < mix.size(); ++i) mix[i] += mix[i - delay] * song.echo;
        }
        return toSamples(std::vector<float>(mix.begin() + static_cast<long>(loop), mix.end()));
    }
}
