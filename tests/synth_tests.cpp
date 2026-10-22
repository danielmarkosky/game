#include <gtest/gtest.h>
#include "src/Audio.h"
#include "src/Config.h"
#include "src/Synth.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace
{
    int peak(const std::vector<sf::Int16>& samples)
    {
        int p = 0;
        for (sf::Int16 s : samples) p = std::max(p, std::abs(static_cast<int>(s)));
        return p;
    }
}

TEST(Synth, ReadsNotesAndChords) {
    EXPECT_EQ(synth::noteNumber("A4"), 69);
    EXPECT_EQ(synth::noteNumber("C4"), 60);
    EXPECT_EQ(synth::noteNumber("C#5"), 73);
    EXPECT_EQ(synth::noteNumber("Bb3"), 58);
    EXPECT_EQ(synth::noteNumber("H2"), -1);
    EXPECT_EQ(synth::chordNotes("Am"), (std::vector<int>{57, 60, 64}));
    EXPECT_EQ(synth::chordNotes("C7"), (std::vector<int>{48, 52, 55, 58}));
    EXPECT_EQ(synth::chordNotes("F#maj7"), (std::vector<int>{54, 58, 61, 65}));
    EXPECT_TRUE(synth::chordNotes("Xm").empty());
}

TEST(Synth, SaysWhatsWrongWithASong) {
    EXPECT_THROW(synth::parseSong("tempo 90\nchords Am\nmelody A4 - -\n"), std::runtime_error); // 3 steps, not 8
    EXPECT_THROW(synth::parseSong("chords Q\n"), std::runtime_error);
    EXPECT_THROW(synth::parseSong("tempo fast\nchords Am\n"), std::runtime_error);
    EXPECT_THROW(synth::parseSong("bass 0.2 x..\nchords Am\n"), std::runtime_error);
    EXPECT_NO_THROW(synth::parseSong("chords Am | F\n"));
}

TEST(Synth, EverySongReadsAndSoundsRight) {
    const auto names = Audio::songNames();
    EXPECT_GE(names.size(), 7u);
    for (const std::string& name : names) {
        SCOPED_TRACE(name);
        std::ifstream file(config::assetPath("music/" + name + ".txt"));
        std::stringstream text;
        text << file.rdbuf();
        const synth::Song song = synth::parseSong(text.str());
        const auto samples = synth::render(song);
        EXPECT_NEAR(static_cast<float>(samples.size()), song.seconds() * synth::RATE, 2.f);
        EXPECT_GT(song.seconds(), 20.f);
        // Loud enough to hear, and never near clipping.
        EXPECT_GT(peak(samples), 4000);
        EXPECT_LT(peak(samples), 31000);
    }
}

TEST(Synth, EffectsAreAsLongAsAsked) {
    const synth::Effect jump{synth::Wave::Square, 260.f, 520.f, 0.12f, 0.002f, 0.08f, 0.22f, 0.f, {}};
    const auto samples = synth::render(jump);
    EXPECT_EQ(samples.size(), static_cast<size_t>(0.12f * synth::RATE));
    EXPECT_GT(peak(samples), 3000);
    EXPECT_LT(std::abs(static_cast<int>(samples.back())), 500); // faded out, no click
}
