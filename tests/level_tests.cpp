#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/Level.h"
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include "src/World.h"
#include <SFML/System.hpp>
#include <set>
#include <unordered_map>
#include <vector>

static constexpr float TILE_SIZE = 8.0f;

namespace {
    std::unordered_map<int, std::unordered_map<int, int>> makeTiles() {
        std::unordered_map<int, std::unordered_map<int, int>> tiles;
        tiles[0][0]   = 1;
        tiles[1][1]   = 2;
        tiles[2][2]   = 1;
        tiles[2][4]   = 2;
        tiles[-1][0]  = 1;
        tiles[-4][5]  = 2;
        tiles[0][-2]  = 1;
        return tiles;
    }
}

TEST(Level_IsSolid, Level_IsSolid) {
    Level lvl(makeTiles());

    EXPECT_TRUE(lvl.isSolid(0, 0));
    EXPECT_TRUE(lvl.isSolid(1, 1));
    EXPECT_TRUE(lvl.isSolid(2, 2));
    EXPECT_TRUE(lvl.isSolid(2, 4));
    EXPECT_TRUE(lvl.isSolid(-1, 0));
    EXPECT_TRUE(lvl.isSolid(-4, 5));
    EXPECT_TRUE(lvl.isSolid(0, -2));

    EXPECT_FALSE(lvl.isSolid(3, 3));
    EXPECT_FALSE(lvl.isSolid(0, 1));
    EXPECT_FALSE(lvl.isSolid(-2, 0));
}

TEST(Level_IsEmptySpace, OverlapsPlatform) {
    Level lvl(makeTiles());
    sf::Vector2f pos{0 * TILE_SIZE + 0.f, 1 * TILE_SIZE + 1.f};
    float height = 6.f;
    float width  = 6.f;
    EXPECT_TRUE(lvl.isEmptySpace(pos, height, width));
}

TEST(Level_IsEmptySpace, OverlapOfOneTile) {
    Level lvl(makeTiles());
    sf::Vector2f pos{1 * TILE_SIZE + 1.f, 1 * TILE_SIZE + 1.f};
    float height = 1.f;
    float width  = 1.f;
    EXPECT_FALSE(lvl.isEmptySpace(pos, height, width));
}

TEST(Level_IsEmptySpace, MultiTileDetection) {
    Level lvl;
    sf::Vector2f pos{-10 * TILE_SIZE + 0.f, -10 * TILE_SIZE + 0.f};
    float height = 200.f;
    float width  = 200.f;
    EXPECT_FALSE(lvl.isEmptySpace(pos, height, width));
}

TEST(Level_LoadFromString, ParsesTilesAndMarkers) {
    // Top line of the text is the top of the level.
    auto lvl = Level::loadFromString(
        "#....G\n"
        "#.C...\n"
        "#S..^%\n"
        "######\n");

    EXPECT_TRUE(lvl->isSolid(0, 3));
    EXPECT_TRUE(lvl->isSolid(3, 0));
    EXPECT_TRUE(lvl->isSolid(5, 1));
    EXPECT_TRUE(lvl->isDangerous(4, 1));
    EXPECT_FALSE(lvl->isSolid(1, 1));

    EXPECT_EQ(lvl->getStartPos(), sf::Vector2f(1 * TILE_SIZE, 1 * TILE_SIZE));

    auto cp = lvl->checkpointAt({2 * TILE_SIZE + 2.f, 2 * TILE_SIZE}, 11.f, 7.f);
    ASSERT_TRUE(cp.has_value());
    EXPECT_EQ(*cp, sf::Vector2f(2 * TILE_SIZE, 2 * TILE_SIZE));
    EXPECT_FALSE(lvl->checkpointAt({1 * TILE_SIZE, 1 * TILE_SIZE}, 11.f, 7.f).has_value());

    EXPECT_TRUE(lvl->isGoal({5 * TILE_SIZE, 3 * TILE_SIZE}, 11.f, 7.f));
    EXPECT_FALSE(lvl->isGoal({1 * TILE_SIZE, 1 * TILE_SIZE}, 11.f, 7.f));

    sf::FloatRect b = lvl->bounds();
    EXPECT_FLOAT_EQ(b.left, 0.f);
    EXPECT_FLOAT_EQ(b.top, 0.f);
    EXPECT_FLOAT_EQ(b.width, 6 * TILE_SIZE);
    EXPECT_FLOAT_EQ(b.height, 4 * TILE_SIZE);
}

TEST(Level_LoadFromString, RequiresStart) {
    EXPECT_THROW(Level::loadFromString("####\n"), std::runtime_error);
}

TEST(Level_IsEmptySpace, BoxEndingOnTileEdgeDoesNotTouchIt) {
    Level lvl(makeTiles());
    // Tile (1, 1) spans x 8..16, y 8..16. A box ending exactly at x = 8 sits flush against it.
    EXPECT_TRUE(lvl.isEmptySpace({1.f, 9.f}, 6.f, 7.f));
    EXPECT_FALSE(lvl.isEmptySpace({1.5f, 9.f}, 6.f, 7.f));
}

TEST(Level_Theme, EveryShippedThemeLoads) {
    std::set<std::string> themes;
    for (const auto& info : loadWorld(config::assetPath("world.ldtk"))) themes.insert(info.theme);
    EXPECT_GE(themes.size(), 5u);
    for (const auto& name : themes) {
        SCOPED_TRACE(name);
        auto theme = Theme::get(name);
        EXPECT_EQ(theme->tiles.getSize(), sf::Vector2u(64, 32)); // 25 tiles, 8 to a row
        for (const auto& layer : theme->layers) EXPECT_EQ(layer.getSize(), sf::Vector2u(320, 180));
    }
    EXPECT_THROW(Theme::get("no_such_theme"), std::runtime_error);
}

TEST(Level_Theme, LevelsShareALoadedTheme) {
    auto a = Theme::get("snow");
    EXPECT_EQ(Theme::get("snow"), a);
    Level lvl;
    EXPECT_EQ(lvl.theme().name, Theme::DEFAULT);
    lvl.setTheme(a);
    EXPECT_EQ(&lvl.theme(), a.get());
}

TEST(Level_Pickups, AreReadFromTheFileAndTouched) {
    auto lvl = Level::loadFromString(
        "#####\n"
        "#S.P#\n"
        "#####\n");
    ASSERT_EQ(lvl->pickups().size(), 1u);
    EXPECT_EQ(lvl->pickups()[0].ability, "rope");
    EXPECT_EQ(lvl->pickups()[0].position, sf::Vector2f(24.f, 8.f));
    EXPECT_FALSE(lvl->isSolid(3, 1));
    EXPECT_EQ(lvl->pickupAt({8.f, 8.f}, 11.f, 7.f), nullptr);
    EXPECT_EQ(lvl->pickupAt({20.f, 8.f}, 11.f, 7.f), &lvl->pickups()[0]);
}

TEST(Level_Gems, AreReadFromTheFileAndTouched) {
    auto lvl = Level::loadFromString(
        "######\n"
        "#S.*.#\n"
        "######\n");
    ASSERT_EQ(lvl->gems().size(), 1u);
    EXPECT_EQ(lvl->gems()[0], sf::Vector2f(24.f, 8.f));
    EXPECT_FALSE(lvl->isSolid(3, 1));
    EXPECT_FALSE(lvl->gemAt({8.f, 8.f}, 11.f, 7.f).has_value());
    EXPECT_FALSE(lvl->gemAt({32.f, 8.f}, 11.f, 7.f).has_value()); // just touching its side
    EXPECT_EQ(lvl->gemAt({28.f, 8.f}, 11.f, 7.f), 0u);
}

TEST(Level_Benches, AreReadFromTheFile) {
    auto lvl = Level::loadFromString(
        "######\n"
        "#S.B.#\n"
        "######\n");
    ASSERT_EQ(lvl->benches().size(), 1u);
    EXPECT_EQ(lvl->benches()[0], sf::Vector2f(24.f, 8.f));
    EXPECT_FALSE(lvl->isSolid(3, 1));
}

TEST(Level_Gates, AreSolidUntilOpened) {
    auto lvl = Level::loadFromString(
        "#####\n"
        "#SL.#\n"
        "##=##\n");
    ASSERT_EQ(lvl->levers().size(), 1u);
    EXPECT_EQ(lvl->levers()[0], sf::Vector2f(16.f, 8.f));
    EXPECT_TRUE(lvl->isSolid(2, 0));
    lvl->setGatesOpen(true);
    EXPECT_FALSE(lvl->isSolid(2, 0));
    EXPECT_TRUE(lvl->isSolid(1, 0));
}

TEST(Level_IsDangerousSpace, OnlyThePointyHalfOfASpikeHurts) {
    // An upward spike at tile (1, 1) on the floor, and a downward one at (3, 2) under a ceiling.
    auto lvl = Level::loadFromString(
        "S..#\n"
        "...^\n"
        ".^..\n"
        "####\n");

    // Up spike: tile y 8..16, only y 8..12 hurts.
    EXPECT_FALSE(lvl->isDangerousSpace({8.f, 12.f}, 11.f, 7.f));
    EXPECT_TRUE(lvl->isDangerousSpace({8.f, 11.f}, 11.f, 7.f));
    // Down spike: tile y 16..24, only y 20..24 hurts.
    EXPECT_FALSE(lvl->isDangerousSpace({24.f, 9.f}, 11.f, 7.f));
    EXPECT_TRUE(lvl->isDangerousSpace({24.f, 10.f}, 11.f, 7.f));
}

TEST(Level_ExitThrough, EverySide) {
    // 5 x 3 tiles: x 0..40, y 0..24.
    auto lvl = Level::loadFromString(
        "#####\n"
        "#S...\n"
        "#####\n");

    EXPECT_EQ(lvl->exitThrough({41.f, 12.f}), Level::Side::Right);
    EXPECT_EQ(lvl->exitThrough({-1.f, 12.f}), Level::Side::Left);
    EXPECT_EQ(lvl->exitThrough({20.f, 25.f}), Level::Side::Top);
    EXPECT_FALSE(lvl->exitThrough({20.f, 12.f}).has_value());
    EXPECT_EQ(lvl->exitThrough({20.f, -1.f}), Level::Side::Bottom);
    EXPECT_FALSE(lvl->exitThrough({40.f, 12.f}).has_value()); // exactly on the edge is still inside
}
