#include <gtest/gtest.h>
#include "src/RoomFile.h"
#include "src/World.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace
{
    // A room as the editor saves it: a door on the left, one on the right joined to camp, a pit.
    const char* LEDGES = "# made by hand\n"
                         "title The Ledges\n"
                         "theme glade\n"
                         "region vale\n"
                         "wind -70\n"
                         "dark 1\n"
                         "join R 2 camp L 8\n"
                         "entity 4 1 dialogue=fen look=fen pose=sit\n"
                         "entity 5 2 kind=pine\n"
                         "tiles\n"
                         "##########\n"
                         "#........#\n"
                         ".....D....\n"
                         "..S.N.....\n"
                         "####..####\n";
}

TEST(RoomFile, ReadsWhatTheEditorWrites) {
    const RoomFile room = RoomFile::parse(LEDGES, "ledges");
    EXPECT_EQ(room.name, "ledges");
    EXPECT_EQ(room.title, "The Ledges");
    EXPECT_EQ(room.theme, "glade");
    EXPECT_EQ(room.region, "vale");
    EXPECT_FLOAT_EQ(room.wind, -70.f);
    EXPECT_TRUE(room.dark);
    ASSERT_EQ(room.width(), 10);
    ASSERT_EQ(room.height(), 5);
    // Rows count up from the bottom.
    EXPECT_EQ(room.at(2, 1), 'S');
    EXPECT_EQ(room.at(0, 0), '#');
    EXPECT_EQ(room.at(4, 0), '.');
    ASSERT_EQ(room.joins.size(), 1u);
    EXPECT_EQ(room.joins[0].side, 'R');
    EXPECT_EQ(room.joins[0].start, 2);
    EXPECT_EQ(room.joins[0].room, "camp");
    EXPECT_EQ(room.joins[0].otherSide, 'L');
    EXPECT_EQ(room.joins[0].otherStart, 8);
    EXPECT_EQ(room.fields.at({4, 1}).at("look"), "fen");
    EXPECT_EQ(room.fields.at({5, 2}).at("kind"), "pine");
    // And writes it back the same.
    const RoomFile again = RoomFile::parse(room.write(), "ledges");
    EXPECT_EQ(again.write(), room.write());
    EXPECT_EQ(again.grid, room.grid);
    EXPECT_EQ(again.fields, room.fields);
}

TEST(RoomFile, SaysWhatsWrongWithAFile) {
    EXPECT_THROW(RoomFile::parse("title x\n", "x"), std::runtime_error);                    // no tiles
    EXPECT_THROW(RoomFile::parse("tiles\n###\n##\n", "x"), std::runtime_error);             // ragged
    EXPECT_THROW(RoomFile::parse("colour red\ntiles\n###\n", "x"), std::runtime_error);     // no such setting
    EXPECT_THROW(RoomFile::parse("join R 2 camp\ntiles\n###\n", "x"), std::runtime_error);  // half a join
    EXPECT_THROW(RoomFile::parse("entity 9 9 kind=pine\ntiles\n###\n", "x"), std::runtime_error); // outside
    EXPECT_THROW(RoomFile::parse("entity 1 0 kind=pine\ntiles\n###\n", "x"), std::runtime_error); // on rock
    EXPECT_NO_THROW(RoomFile::parse("entity 1 0 kind=pine\ntiles\n#D#\n", "x"));
    EXPECT_TRUE(RoomFile::looksLikeOne(LEDGES));
    EXPECT_FALSE(RoomFile::looksLikeOne("#####\n#S..#\n#####\n"));
}

TEST(RoomFile, FindsTheDoorsInTheOutsideWalls) {
    const RoomFile room = RoomFile::parse(LEDGES, "ledges");
    const auto doors = room.doors();
    ASSERT_EQ(doors.size(), 3u);
    EXPECT_EQ(doors[0].side, 'L'); // rows 1 and 2: not the corner
    EXPECT_EQ(doors[0].start, 1);
    EXPECT_EQ(doors[0].length, 2);
    EXPECT_EQ(doors[1].side, 'R');
    EXPECT_EQ(doors[1].start, 1);
    EXPECT_EQ(doors[1].length, 2);
    EXPECT_EQ(doors[2].side, 'B');
    EXPECT_EQ(doors[2].start, 4);
    EXPECT_EQ(doors[2].length, 2);
    // Water and platforms are no wall; slick stone and ice are.
    RoomFile walled = RoomFile::blank("w", 8, 6);
    walled.set(0, 2, 'w');
    walled.set(7, 2, 'o');
    walled.set(7, 3, '~');
    ASSERT_EQ(walled.doors().size(), 1u);
    EXPECT_EQ(walled.doors()[0].side, 'L');
}

TEST(RoomFile, GrowingMovesTheRightAndTopWallsOut) {
    RoomFile room = RoomFile::parse(LEDGES, "ledges");
    room.resize(14, 8);
    EXPECT_EQ(room.width(), 14);
    EXPECT_EQ(room.height(), 8);
    EXPECT_EQ(room.at(2, 1), 'S');
    EXPECT_EQ(room.fields.at({4, 1}).at("look"), "fen");
    // The right wall's door is still on the right, as high up; the new tiles carry on from the
    // last ones inside, walled, but without what's standing there.
    EXPECT_EQ(room.at(13, 1), '.');
    EXPECT_EQ(room.at(13, 3), '#');
    EXPECT_EQ(room.at(11, 4), '.');
    EXPECT_EQ(room.at(11, 7), '#');
    EXPECT_EQ(room.at(11, 0), '#');
    EXPECT_EQ(room.at(13, 5), '#');
    RoomFile floored = RoomFile::parse("tiles\n######\n#....#\n#S..D#\n##..##\n######\n", "f");
    floored.fields[{4, 2}] = {{"kind", "pine"}};
    floored.resize(8, 6);
    EXPECT_EQ(floored.at(5, 1), '#'); // the floor goes on
    EXPECT_EQ(floored.at(5, 2), '.'); // the prop doesn't
    EXPECT_EQ(floored.at(6, 4), '.');
    EXPECT_EQ(floored.at(2, 4), '.');
    EXPECT_EQ(floored.at(2, 5), '#');
    EXPECT_EQ(floored.fields.size(), 1u);
    room.resize(10, 5);
    EXPECT_EQ(room.write(), RoomFile::parse(LEDGES, "ledges").write());
}

TEST(RoomFile, GrowsOnTheLeftAndBottomToo) {
    RoomFile room = RoomFile::parse(LEDGES, "ledges");
    room.grow(3, 0, 2, 0);
    EXPECT_EQ(room.width(), 13);
    EXPECT_EQ(room.height(), 7);
    // Everything moved right 3 and up 2, its fields too, and the left wall with its door went out.
    EXPECT_EQ(room.at(5, 3), 'S');
    EXPECT_EQ(room.fields.at({7, 3}).at("look"), "fen");
    EXPECT_EQ(room.at(0, 3), '.');
    EXPECT_EQ(room.at(0, 4), '.');
    EXPECT_EQ(room.at(0, 2), '#');
    // The bottom wall went down 2, with the pit in it; the new rows carry on from the bottom row inside.
    EXPECT_EQ(room.at(7, 0), '.');
    EXPECT_EQ(room.at(3, 0), '#');
    EXPECT_EQ(room.at(3, 1), '.');
    EXPECT_EQ(room.at(3, 2), '.');
    // The join's door is 2 higher now.
    EXPECT_EQ(room.joins[0].start, 4);
    ASSERT_EQ(room.doors()[1].side, 'R');
    EXPECT_EQ(room.doors()[1].start, 3);
    // And back.
    room.grow(-3, 0, -2, 0);
    EXPECT_EQ(room.write(), RoomFile::parse(LEDGES, "ledges").write());
    // Never smaller than its walls round a tile.
    room.grow(-50, -50, -50, -50);
    EXPECT_EQ(room.width(), RoomFile::MIN_SIZE);
    EXPECT_EQ(room.height(), RoomFile::MIN_SIZE);
}

TEST(RoomFile, PlaysWithItsThingsAndSettings) {
    const LevelInfo info = RoomFile::parse(LEDGES, "ledges").toLevel();
    EXPECT_EQ(info.name, "ledges");
    EXPECT_EQ(info.title, "The Ledges");
    EXPECT_EQ(info.theme, "glade");
    EXPECT_EQ(info.region, "vale");
    const Level& level = *info.level;
    EXPECT_EQ(level.getStartPos(), sf::Vector2f(16.f, 8.f));
    ASSERT_EQ(level.npcs().size(), 1u);
    EXPECT_EQ(level.npcs()[0].dialogue, "fen");
    EXPECT_TRUE(level.npcs()[0].sitting);
    ASSERT_EQ(level.props().size(), 1u);
    EXPECT_EQ(level.props()[0].kind, "pine");
    EXPECT_FLOAT_EQ(level.wind(), -70.f);
    EXPECT_TRUE(level.isDark());
    EXPECT_FALSE(level.isSolid(2, 1)); // the Start's tile is empty
    EXPECT_TRUE(level.isSolid(0, 0));
    EXPECT_THROW(RoomFile::blank("empty", 8, 6).toLevel(), std::runtime_error);
    EXPECT_NO_THROW(RoomFile::blank("empty", 8, 6).toLevel(false));
}

TEST(RoomFile, TheGamePlaysOneOnItsOwn) {
    const auto path = std::filesystem::temp_directory_path() / "ledges.txt";
    std::ofstream(path) << LEDGES;
    const LevelInfo info = loadSingleLevel(path.string());
    EXPECT_EQ(info.title, "The Ledges");
    EXPECT_EQ(info.theme, "glade");
    EXPECT_EQ(info.level->npcs().size(), 1u);
    std::filesystem::remove(path);
}
