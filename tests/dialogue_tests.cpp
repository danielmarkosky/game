#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/Dialogue.h"
#include "src/Player.h"
#include "src/Text.h"
#include "src/World.h"
#include <filesystem>
#include <fstream>
#include <set>

namespace
{
    // Runs a conversation to the end, picking `choices` in turn, and returns the lines said.
    std::vector<std::string> play(const Conversation& c, std::vector<size_t> choices, std::set<std::string>& flags)
    {
        std::vector<std::string> said;
        DialogueRunner run(c, [&](const std::string& flag) { flags.insert(flag); });
        while (!run.finished()) {
            if (run.current().kind == DialogueStep::Kind::Choice) {
                run.choose(choices.front());
                choices.erase(choices.begin());
            } else {
                said.push_back(run.current().speaker + ": " + run.current().text);
                run.next();
            }
        }
        return said;
    }
}

TEST(Dialogue, ReadsSpeakersLinesAndConversations) {
    const Dialogue d = Dialogue::parse(
        "# a comment\n"
        "Wren: Hello there.\n"
        "Still me talking.\n"
        "You: Hi: nice to meet you.\n"
        "\n"
        "--- climbing_pick !met\n"
        "Wren: Nice pick.\n");
    ASSERT_EQ(d.conversations().size(), 2u);
    std::set<std::string> flags;
    EXPECT_EQ(play(d.conversations()[0], {}, flags),
              (std::vector<std::string>{"Wren: Hello there.", "Wren: Still me talking.", "You: Hi: nice to meet you."}));
    EXPECT_EQ(d.conversations()[1].needs, (std::vector<std::string>{"climbing_pick", "!met"}));
}

TEST(Dialogue, ChoicesLeadToTheirLabelsAndSetFlags) {
    const Dialogue d = Dialogue::parse(
        "Wren: Why are you here?\n"
        "? My sister.     -> sister\n"
        "? Just passing.  -> passing\n"
        "= sister\n"
        "Wren: Ash? I knew her.\n"
        "set told_wren\n"
        "-> after\n"
        "= passing\n"
        "Wren: Suit yourself.\n"
        "= after\n"
        "Wren: Off you go.\n");
    const Conversation& c = d.conversations()[0];
    std::set<std::string> flags;
    EXPECT_EQ(play(c, {0}, flags), (std::vector<std::string>{"Wren: Why are you here?", "Wren: Ash? I knew her.", "Wren: Off you go."}));
    EXPECT_EQ(flags, std::set<std::string>{"told_wren"});
    flags.clear();
    // The other way stops at the next label.
    EXPECT_EQ(play(c, {1}, flags), (std::vector<std::string>{"Wren: Why are you here?", "Wren: Suit yourself."}));
    EXPECT_TRUE(flags.empty());
}

TEST(Dialogue, PicksTheFirstNewConversationTheFactsAllow) {
    const Dialogue d = Dialogue::parse("--- !pick\nA: one\n--- !pick\nA: two\n--- pick\nA: three\n--- pick\nA: four\n");
    std::set<size_t> had;
    bool hasPick = false;
    auto next = [&] { return d.next([&](size_t n) { return had.count(n) > 0; }, [&](const std::string&) { return hasPick; }); };

    EXPECT_EQ(next(), 0u);
    had.insert(0);
    EXPECT_EQ(next(), 1u);
    had.insert(1);
    EXPECT_EQ(next(), 1u); // nothing new without the pick: the last one again
    hasPick = true;
    EXPECT_EQ(next(), 2u);
    had.insert(2);
    EXPECT_EQ(next(), 3u);
    had.insert(3);
    EXPECT_EQ(next(), 3u);
    EXPECT_FALSE(Dialogue::parse("--- pick\nA: hi\n").next([](size_t) { return false; }, [](const std::string&) { return false; }).has_value());
}

TEST(Dialogue, EveryCharacterInTheWorldHasSomethingToSay) {
    for (const auto& room : loadWorld(config::assetPath("world.ldtk"))) {
        for (const Npc& npc : room.level->npcs()) {
            SCOPED_TRACE(room.name + ": " + npc.dialogue);
            const Dialogue d = Dialogue::load(npc.dialogue);
            // Someone (one of the looks), or one of Ash's notes.
            EXPECT_TRUE(npc.look == "note" || std::find(Player::LOOKS.begin(), Player::LOOKS.end(), npc.look) != Player::LOOKS.end()) << npc.look;
            for (const auto& conversation : d.conversations()) {
                for (const auto& step : conversation.steps) {
                    // Only what the font can draw.
                    std::string shown = step.speaker + step.text;
                    for (const auto& option : step.options) shown += option.first;
                    for (char c : shown) EXPECT_TRUE(c >= ' ' && c <= '~') << shown;
                }
            }
        }
    }
}

TEST(Dialogue, ThereIsAlwaysAnEnding) {
    // The end screen shows the first ending the story's flags allow: the last needs none.
    const Dialogue endings = Dialogue::load("endings");
    ASSERT_FALSE(endings.conversations().empty());
    EXPECT_TRUE(endings.conversations().back().needs.empty());
}

TEST(Dialogue, WhatYouTellAshDecidesTheEnding) {
    const Dialogue endings = Dialogue::load("endings");
    auto endingFor = [&](const std::set<std::string>& facts) {
        for (const auto& c : endings.conversations()) {
            if (Dialogue::allowed(c, [&](const std::string& f) { return facts.count(f) > 0; })) return c.steps.front().text;
        }
        return std::string();
    };
    // Each answer in ash.txt sets one of these.
    std::set<std::string> seen;
    for (const char* answer : {"ash_home", "ash_stay", "ash_letgo"}) seen.insert(endingFor({answer, "ash_answered"}));
    seen.insert(endingFor({}));
    EXPECT_EQ(seen.size(), 4u); // all different
}

TEST(Dialogue, ALabelThatIsntThereIsAnError) {
    const auto path = std::filesystem::path(config::assetPath("dialogue")) / "test_broken.txt";
    std::ofstream(path) << "A: hi\n? go -> nowhere\n";
    EXPECT_THROW(Dialogue::load("test_broken"), std::runtime_error);
    std::filesystem::remove(path);
}

TEST(Text, WrapsAtSpacesToFitAWidth) {
    const std::string s = "The quick brown fox jumps over the lazy dog";
    const auto lines = text::wrap(s, 60.f);
    ASSERT_GT(lines.size(), 1u);
    std::string joined;
    for (const auto& line : lines) {
        EXPECT_LE(text::width(line), 60.f) << line;
        joined += (joined.empty() ? "" : " ") + line;
    }
    EXPECT_EQ(joined, s);
    EXPECT_EQ(text::wrap("Pneumonoultramicroscopic", 20.f).size(), 1u); // too long to break: on its own line
    EXPECT_GT(text::width("m"), text::width("i"));
    EXPECT_EQ(text::width("ab"), text::width("a") + 1.f + text::width("b"));
}

TEST(Dialogue, CommandsGiveTakeShopAndRace) {
    EXPECT_TRUE(Dialogue::isCommand("give gems 5"));
    EXPECT_TRUE(Dialogue::isCommand("give outfit scholar"));
    EXPECT_TRUE(Dialogue::isCommand("take item moss_page"));
    EXPECT_TRUE(Dialogue::isCommand("shop"));
    EXPECT_TRUE(Dialogue::isCommand("race garden"));
    EXPECT_FALSE(Dialogue::isCommand("give"));
    EXPECT_FALSE(Dialogue::isCommand("give hugs 5"));
    EXPECT_FALSE(Dialogue::isCommand("shop now"));

    const Dialogue d = Dialogue::parse("---\nTuck: Have a look.\nshop\nTuck: Here, a gift.\ngive gems 2\nset met_tuck\n");
    std::vector<std::string> done;
    std::set<std::string> flags;
    DialogueRunner run(d.conversations()[0], [&](const std::string& f) { flags.insert(f); }, [&](const std::string& c) { done.push_back(c); });
    while (!run.finished()) run.next();
    EXPECT_EQ(done, (std::vector<std::string>{"shop", "give gems 2"}));
    EXPECT_EQ(flags, std::set<std::string>{"met_tuck"});
}
