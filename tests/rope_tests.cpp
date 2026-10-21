#include <gtest/gtest.h>
#include "src/Rope.h"
#include "src/utils.h"

namespace
{
    std::shared_ptr<Level> emptyLevel()
    {
        return std::make_shared<Level>(std::unordered_map<int, std::unordered_map<int, int>>{});
    }
}

TEST(RopeTest, Initialization_LengthAndEnds) {
    sf::Vector2f start(0.f, 0.f);
    sf::Vector2f anchor(0.f, 20.f);

    Rope rope(start, anchor, emptyLevel());
    const auto& nodes = rope.getNodes();

    EXPECT_FLOAT_EQ(rope.length(), 20.f);
    EXPECT_EQ(rope.anchor(), anchor);
    ASSERT_EQ(nodes.size(), static_cast<size_t>(Rope::NUM_SEGMENTS + 1));
    EXPECT_EQ(nodes.front().position, start);
    EXPECT_EQ(nodes.back().position, anchor);
    EXPECT_FALSE(nodes.front().isFixed);
    EXPECT_TRUE(nodes.back().isFixed);
}

TEST(RopeTest, LengthIsClamped) {
    Rope tiny({0.f, 0.f}, {0.f, 1.f}, emptyLevel());
    EXPECT_FLOAT_EQ(tiny.length(), Rope::MIN_LENGTH);

    Rope rope({0.f, 0.f}, {0.f, 50.f}, emptyLevel());
    rope.setLength(1000.f);
    EXPECT_FLOAT_EQ(rope.length(), Rope::MAX_LENGTH);
    rope.setLength(-5.f);
    EXPECT_FLOAT_EQ(rope.length(), Rope::MIN_LENGTH);
}

TEST(RopeTest, SlackRopeDoesNothing) {
    Rope rope({0.f, 0.f}, {0.f, 50.f}, emptyLevel());
    sf::Vector2f inside(10.f, 30.f);
    sf::Vector2f velocity(-40.f, -70.f);

    EXPECT_FALSE(rope.isTaut(inside));
    EXPECT_EQ(rope.constrainPosition(inside), inside);
    EXPECT_EQ(rope.constrainVelocity(inside, velocity), velocity);
}

TEST(RopeTest, TautRopeRemovesOutwardVelocityOnly) {
    // Hanging straight below the anchor.
    Rope rope({0.f, 0.f}, {0.f, 50.f}, emptyLevel());
    sf::Vector2f pos(0.f, 0.f);
    ASSERT_TRUE(rope.isTaut(pos));

    // Falling further down is stopped, moving sideways is not.
    sf::Vector2f v = rope.constrainVelocity(pos, {30.f, -100.f});
    EXPECT_NEAR(v.x, 30.f, 1e-4f);
    EXPECT_NEAR(v.y, 0.f, 1e-4f);

    // Moving back towards the anchor is fine.
    v = rope.constrainVelocity(pos, {0.f, 60.f});
    EXPECT_NEAR(v.y, 60.f, 1e-4f);
}

TEST(RopeTest, ConstrainPositionPullsBackOntoCircle) {
    Rope rope({0.f, 0.f}, {0.f, 50.f}, emptyLevel());
    sf::Vector2f pulled = rope.constrainPosition({30.f, -40.f});
    EXPECT_NEAR(distance(pulled, rope.anchor()), 50.f, 1e-3f);
    // Same direction from the anchor as before.
    EXPECT_NEAR(pulled.x / (pulled.y - 50.f), 30.f / (-40.f - 50.f), 1e-4f);
}

TEST(RopeTest, VisualChainStaysPinnedAndSags) {
    Rope rope({0.f, 0.f}, {40.f, 0.f}, emptyLevel());
    rope.setLength(60.f); // slack

    for (int ii = 0; ii < 240; ++ii) {
        rope.update({0.f, 0.f}, 1.f / 120.f);
    }
    const auto& nodes = rope.getNodes();
    EXPECT_EQ(nodes.front().position, sf::Vector2f(0.f, 0.f));
    EXPECT_EQ(nodes.back().position, sf::Vector2f(40.f, 0.f));
    EXPECT_LT(nodes[nodes.size() / 2].position.y, -5.f);

    float total = 0.f;
    for (size_t ii = 0; ii + 1 < nodes.size(); ++ii) {
        total += distance(nodes[ii].position, nodes[ii + 1].position);
    }
    EXPECT_NEAR(total, 60.f, 3.f);
}

TEST(RopeTest, VisualChainIsStraightWhenTaut) {
    Rope rope({0.f, 0.f}, {40.f, 0.f}, emptyLevel());
    // Player swings to a point at full length below the anchor.
    rope.update({40.f, -40.f}, 1.f / 120.f);

    for (const auto& node : rope.getNodes()) {
        EXPECT_NEAR(node.position.x, 40.f, 1e-3f);
    }
}

TEST(RopeTest, WrapsAroundCornersAndUnwraps) {
    // One rock tile at x 40..48, y 40..48.
    std::unordered_map<int, std::unordered_map<int, int>> tiles;
    tiles[5][5] = 1;
    auto level = std::make_shared<Level>(tiles);

    Rope rope({60.f, 60.f}, {0.f, 44.f}, level);
    const float total = rope.length();

    // Nothing in the way.
    rope.wrap({60.f, 60.f}, {60.f, 56.f});
    EXPECT_EQ(rope.pivots().size(), 1u);

    // Swinging down past the tile: the rope goes over its top and catches on both top corners.
    rope.wrap({60.f, 56.f}, {60.f, 44.f});
    ASSERT_EQ(rope.pivots().size(), 3u);
    EXPECT_NEAR(rope.pivots()[1].x, 40.f, 0.5f);
    EXPECT_NEAR(rope.pivots()[1].y, 48.f, 0.5f);
    EXPECT_NEAR(rope.pivots()[2].x, 48.f, 0.5f);
    EXPECT_NEAR(rope.pivots()[2].y, 48.f, 0.5f);
    EXPECT_EQ(rope.pivot(), rope.pivots()[2]);
    // The wrapped part is used up; the player swings around the last corner with what's left.
    EXPECT_NEAR(rope.freeLength(), total - 40.2f - 8.4f, 0.5f);
    EXPECT_FLOAT_EQ(rope.length(), total);

    // Swinging back up unwraps both.
    rope.wrap({60.f, 44.f}, {60.f, 60.f});
    EXPECT_EQ(rope.pivots().size(), 1u);
    EXPECT_FLOAT_EQ(rope.freeLength(), total);
}

TEST(RopeTest, ReelingStopsAtTheWrappedPart) {
    std::unordered_map<int, std::unordered_map<int, int>> tiles;
    tiles[5][5] = 1;
    auto level = std::make_shared<Level>(tiles);

    Rope rope({60.f, 60.f}, {0.f, 44.f}, level);
    rope.wrap({60.f, 60.f}, {60.f, 44.f});
    ASSERT_EQ(rope.pivots().size(), 3u);

    rope.setLength(0.f);
    EXPECT_NEAR(rope.freeLength(), Rope::MIN_LENGTH, 1e-3f);
}

TEST(RopeTest, SlackChainRestsOnTheFloor) {
    // A floor of rock below y = 0.
    std::unordered_map<int, std::unordered_map<int, int>> tiles;
    for (int x = -5; x <= 10; ++x) tiles[x][-1] = 1;
    auto level = std::make_shared<Level>(tiles);

    // Both ends just above the floor with lots of slack: it would sag through without collision.
    Rope rope({0.f, 2.f}, {40.f, 2.f}, level);
    rope.setLength(80.f);
    for (int ii = 0; ii < 240; ++ii) rope.update({0.f, 2.f}, 1.f / 120.f);

    for (const auto& node : rope.getNodes()) {
        EXPECT_GE(node.position.y, -0.01f);
    }
}
