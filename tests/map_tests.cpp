#include <gtest/gtest.h>
#include "src/WorldMap.h"

namespace
{
    // Two 4 x 3 rooms side by side, joined by a gap, the second one 1 tile higher.
    WorldMap twoRooms()
    {
        auto a = Level::loadFromString(
            "####\n"
            "#S..\n"
            "####\n");
        auto b = Level::loadFromString(
            "####\n"
            "..S#\n"
            "####\n");
        return WorldMap({{a, {0, 0}}, {b, {4, 1}}});
    }
}

TEST(WorldMap, RoomsSitAtTheirPositions) {
    WorldMap map = twoRooms();
    EXPECT_EQ(map.roomRect(1), sf::FloatRect(4.f, 1.f, 4.f, 3.f));
    EXPECT_EQ(map.roomAt({1.5f, 1.5f}), 0u);
    EXPECT_EQ(map.roomAt({5.5f, 3.5f}), 1u);
    EXPECT_FALSE(map.roomAt({1.5f, 3.5f}).has_value());
}

TEST(WorldMap, OnlyRevealedRoomsCountTowardsTheBounds) {
    WorldMap map = twoRooms();
    map.reveal({true, false});
    EXPECT_EQ(map.shownBounds(), sf::FloatRect(0.f, 0.f, 4.f, 3.f));
    map.reveal({true, true});
    EXPECT_EQ(map.shownBounds(), sf::FloatRect(0.f, 0.f, 8.f, 4.f));
}

TEST(WorldMap, ScreenAndWorldConvertBothWays) {
    WorldMap map = twoRooms();
    for (int zoom : WorldMap::ZOOMS) {
        const WorldMap::View view{{10.f, 20.f, 100.f, 60.f}, {4.f, 2.f}, zoom};
        EXPECT_EQ(map.toScreen({4.f, 2.f}, view), sf::Vector2f(60.f, 50.f)); // the centre is in the middle
        // Further right on the map is further right on screen, higher up is further up.
        EXPECT_EQ(map.toScreen({4.f + zoom, 2.f + zoom}, view), sf::Vector2f(61.f, 49.f));
        const sf::Vector2f tile(6.5f, -1.25f);
        EXPECT_EQ(map.toWorld(map.toScreen(tile, view), view), tile);
    }
}
