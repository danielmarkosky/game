#include <gtest/gtest.h>
#include "src/Level.h"
#include <SFML/System.hpp>
#include <unordered_map>

static constexpr float TILE_SIZE = 8.0f;

namespace {
    std::unordered_map<int, std::unordered_map<int,int>> makeTiles()
    {
        std::unordered_map<int, std::unordered_map<int,int>> tiles;
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
