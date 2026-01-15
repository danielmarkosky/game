#include <gtest/gtest.h>
#include "src/utils.h"
#include <SFML/System.hpp>

TEST(Utils_ViewCoordinates, ZeroPositionAndCenter) {
    sf::Vector2f pos{0.f, 0.f};
    sf::Vector2f center{0.f, 0.f};
    sf::Vector2f out = viewCoordinates(pos, center);
    EXPECT_FLOAT_EQ(out.x, 160.f);
    EXPECT_FLOAT_EQ(out.y, 110.f);
}

TEST(Utils_ViewCoordinates, NonZeroValues) {
    sf::Vector2f pos{10.f, 5.f};
    sf::Vector2f center{2.f, 3.f};
    sf::Vector2f out = viewCoordinates(pos, center);
    EXPECT_FLOAT_EQ(out.x, 168.f);
    EXPECT_FLOAT_EQ(out.y, 108.f);
}

TEST(Utils_TailNumbers, PositiveAndBoundary) {
    auto p0 = tailNumbers({0.f, 0.f});
    EXPECT_EQ(p0.first, 0);
    EXPECT_EQ(p0.second, 0);

    auto p1 = tailNumbers({7.9f, 15.9f});
    EXPECT_EQ(p1.first, 0);
    EXPECT_EQ(p1.second, 1);

    auto p2 = tailNumbers({8.0f, 16.0f});
    EXPECT_EQ(p2.first, 1);
    EXPECT_EQ(p2.second, 2);
}

TEST(Utils_TailNumbers, NegativeFlooring) {
    auto p0 = tailNumbers({-4.0f, -4.0f});
    EXPECT_EQ(p0.first, -1);
    EXPECT_EQ(p0.second, -1);

    auto p1 = tailNumbers({-8.0f, -16.0f});
    EXPECT_EQ(p1.first, -1);
    EXPECT_EQ(p1.second, -2);

    auto p2 = tailNumbers({-9.0f, -17.0f});
    EXPECT_EQ(p2.first, -2);
    EXPECT_EQ(p2.second, -3);
}

TEST(Utils_Distance, BasicDistances) {
    sf::Vector2f p1{0.f, 0.f};
    sf::Vector2f p2{0.f, -1.f};
    double dist = distance(p1, p2);
    EXPECT_DOUBLE_EQ(dist, 1.0);

    sf::Vector2f p3{-1.f, -1.f};
    sf::Vector2f p4{2.f, 3.f};
    dist = distance(p3, p4);
    EXPECT_DOUBLE_EQ(dist, 5.0);
}

namespace
{
    std::unordered_map<int, std::unordered_map<int, int>> makeTiles()
    {
        std::unordered_map<int, std::unordered_map<int, int>> tiles;
        tiles[0][0]   = 1;
        tiles[1][1]   = 2;
        tiles[2][2]   = 1;
        tiles[2][4]   = 2;
        tiles[0][-2] = 1;
        return tiles;
    }
}

TEST(nearestSolidPoint, FindsSolidPoint) {
    auto level = std::make_shared<Level>(makeTiles());

    sf::Vector2f from{0.f, -20.f};
    sf::Vector2f to{0.f, 100.f};

    auto result = nearestSolidPoint(from, to, level);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(tailNumbers(*result).first, 0);
    EXPECT_EQ(tailNumbers(*result).second, -2);
}

TEST(nearestSolidPoint, NoSolidPoint) {
    auto level = std::make_shared<Level>(makeTiles());

    sf::Vector2f from{100.f, 100.f};
    sf::Vector2f to{200.f, 200.f};

    auto result = nearestSolidPoint(from, to, level);
    EXPECT_FALSE(result.has_value());
}

TEST(nearestSolidPoint, FindsSolidPointAlongDiagonal) {
    auto level = std::make_shared<Level>(makeTiles());

    sf::Vector2f from{-20.f, -20.f};
    sf::Vector2f to{100.f, 100.f};

    auto result = nearestSolidPoint(from, to, level);
    ASSERT_TRUE(result.has_value());

    auto [tx, ty] = tailNumbers(*result);
    EXPECT_TRUE(level->isSolid(tx, ty));
}

TEST(Utils_WorldCoordinates, ZeroPositionAndCenter) {
    sf::Vector2f screenPos{160.f, 110.f};
    sf::Vector2f center{0.f, 0.f};
    sf::Vector2f out = worldCoordinates(screenPos, center);

    EXPECT_FLOAT_EQ(out.x, 0.f);
    EXPECT_FLOAT_EQ(out.y, 0.f);
}

TEST(Utils_WorldCoordinates, InverseOfViewCoordinates) {
    sf::Vector2f originalWorldPos{10.f, 5.f};
    sf::Vector2f center{2.f, 3.f};

    sf::Vector2f screenPos = viewCoordinates(originalWorldPos, center);

    sf::Vector2f backToWorld = worldCoordinates(screenPos, center);

    EXPECT_FLOAT_EQ(backToWorld.x, originalWorldPos.x);
    EXPECT_FLOAT_EQ(backToWorld.y, originalWorldPos.y);
}

TEST(Utils_WorldCoordinates, SpecificValues) {
    sf::Vector2f screenPos{170.f, 100.f};
    sf::Vector2f center{0.f, 0.f};

    sf::Vector2f out = worldCoordinates(screenPos, center);

    EXPECT_FLOAT_EQ(out.x, 10.f);
    EXPECT_FLOAT_EQ(out.y, 10.f);
}
