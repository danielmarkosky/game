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
