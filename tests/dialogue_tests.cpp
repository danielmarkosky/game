#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/Dialogue.h"
#include "src/Player.h"
#include "src/Text.h"
#include "src/World.h"
#include <set>

TEST(Dialogue, ReadsSpeakersLinesAndConversations) {
    const Dialogue d = Dialogue::parse(
        "# a comment\n"
        "Wren: Hello there.\n"
        "Still me talking.\n"
        "You: Hi: nice to meet you.\n"
        "\n"
        "--- climbing_pick\n"
        "Wren: Nice pick.\n");
    ASSERT_EQ(d.conversations().size(), 2u);
    const auto& first = d.conversations()[0].lines;
    ASSERT_EQ(first.size(), 3u);
    EXPECT_EQ(first[0].speaker, "Wren");
    EXPECT_EQ(first[0].text, "Hello there.");
    EXPECT_EQ(first[1].speaker, "Wren"); // no name: the same speaker as before
    EXPECT_EQ(first[1].text, "Still me talking.");
    EXPECT_EQ(first[2].speaker, "You");
    EXPECT_EQ(first[2].text, "Hi: nice to meet you.");
    EXPECT_EQ(d.conversations()[1].needs, "climbing_pick");
}

TEST(Dialogue, PicksTheFirstNewConversationTheAbilitiesAllow) {
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
            EXPECT_TRUE(std::find(Player::SKINS.begin(), Player::SKINS.end(), npc.look) != Player::SKINS.end());
            for (const auto& conversation : d.conversations()) {
                for (const auto& line : conversation.lines) {
                    // Only what the font can draw.
                    for (char c : line.speaker + line.text) EXPECT_TRUE(c >= ' ' && c <= '~') << line.text;
                }
            }
        }
    }
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
