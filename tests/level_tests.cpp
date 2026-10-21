#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/Level.h"
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include "src/Game.h"
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

TEST(Level_LoadFromFile, AllShippedLevelsLoad) {
    auto levels = loadLevelList(config::assetPath("levels/levels.txt"));
    EXPECT_GE(levels.size(), 10u);
    for (const auto& info : levels) {
        SCOPED_TRACE(info.path);
        EXPECT_FALSE(info.title.empty());
        auto lvl = Level::loadFromFile(info.path);
        EXPECT_GT(lvl->bounds().width, 0.f);
    }
}

TEST(Level_LoadLevelList, ParsesFilesAndTitles) {
    const auto path = std::filesystem::temp_directory_path() / "game_test_levels.txt";
    {
        std::ofstream file(path);
        file << "# comment\n\nfirst.txt  The First One\nsecond.txt Two\n";
    }
    auto levels = loadLevelList(path.string());
    std::filesystem::remove(path);

    ASSERT_EQ(levels.size(), 2u);
    EXPECT_EQ(levels[0].path, (path.parent_path() / "first.txt").string());
    EXPECT_EQ(levels[0].title, "The First One");
    EXPECT_EQ(levels[1].title, "Two");
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

TEST(Level_ExitThrough, LeftRightAndTopButNotBottom) {
    // 5 x 3 tiles: x 0..40, y 0..24.
    auto lvl = Level::loadFromString(
        "#####\n"
        "#S...\n"
        "#####\n");

    EXPECT_EQ(lvl->exitThrough({41.f, 12.f}), Level::Side::Right);
    EXPECT_EQ(lvl->exitThrough({-1.f, 12.f}), Level::Side::Left);
    EXPECT_EQ(lvl->exitThrough({20.f, 25.f}), Level::Side::Top);
    EXPECT_FALSE(lvl->exitThrough({20.f, 12.f}).has_value());
    EXPECT_FALSE(lvl->exitThrough({40.f, 12.f}).has_value()); // exactly on the edge is still inside
    EXPECT_FALSE(lvl->exitThrough({20.f, -30.f}).has_value()); // falling out isn't leaving
}

TEST(Level_ExitThrough, EveryShippedLevelHasAWayOut) {
    // Flood fills the empty tiles from the start. Some must lead out through the left, right or
    // top edge, or the level could never be finished. (This doesn't check the jumps are possible.)
    for (const auto& info : loadLevelList(config::assetPath("levels/levels.txt"))) {
        SCOPED_TRACE(info.path);
        auto lvl = Level::loadFromFile(info.path);
        const sf::FloatRect b = lvl->bounds();
        const int minX = static_cast<int>(b.left / TILE_SIZE), maxX = static_cast<int>((b.left + b.width) / TILE_SIZE) - 1;
        const int minY = static_cast<int>(b.top / TILE_SIZE), maxY = static_cast<int>((b.top + b.height) / TILE_SIZE) - 1;
        const sf::Vector2f start = lvl->getStartPos();

        std::vector<std::pair<int, int>> todo = {{static_cast<int>(start.x / TILE_SIZE), static_cast<int>(start.y / TILE_SIZE)}};
        std::set<std::pair<int, int>> seen(todo.begin(), todo.end());
        bool foundExit = false;
        while (!todo.empty() && !foundExit) {
            auto [x, y] = todo.back();
            todo.pop_back();
            if (x < minX || x > maxX || y > maxY) {
                foundExit = true;
                break;
            }
            if (y < minY) continue; // fell out of the bottom
            for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                std::pair<int, int> next{x + dx, y + dy};
                if (lvl->isSolid(next.first, next.second) || !seen.insert(next).second) continue;
                todo.push_back(next);
            }
        }
        EXPECT_TRUE(foundExit);
    }
}
