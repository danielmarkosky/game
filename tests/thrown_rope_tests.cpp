#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/Level.h"
#include "src/Physics.h"
#include "src/ThrownRope.h"
#include "src/utils.h"

namespace
{
    constexpr float DT = config::FIXED_DT;

    // 30 x 10. The floor's top is at y = 8, and a ledge 3 tiles high (top at y = 32) stands from
    // x = 120 to 160.
    const std::string ROOM =
        "##############################\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#..............#####.........#\n"
        "#..............#####.........#\n"
        "#..............#####.........#\n"
        "#.S..........................#\n"
        "##############################\n";

    bool inRock(const Level& level, sf::Vector2f p)
    {
        const auto [x, y] = tailNumbers(p);
        return level.isSolid(x, y);
    }

    float pathLength(const std::vector<sf::Vector2f>& path)
    {
        float total = 0.f;
        for (size_t ii = 1; ii < path.size(); ++ii) total += distance(path[ii - 1], path[ii]);
        return total;
    }
}

TEST(ThrownRope, KeepsTheLengthItWasLetGoAt) {
    auto level = Level::loadFromString(ROOM);
    const std::vector<sf::Vector2f> path = {{140.f, 33.f}, {118.f, 33.f}, {118.f, 9.f}, {60.f, 9.f}};
    ThrownRope rope(path, level);
    EXPECT_NEAR(rope.length(), pathLength(path), ThrownRope::SEGMENT);
    EXPECT_EQ(rope.hook(), path.front());
}

TEST(ThrownRope, FallsAndLiesOnTheGroundWithoutGoingIntoIt) {
    auto level = Level::loadFromString(ROOM);
    // Let go in the air, across the top of the ledge and beyond it.
    ThrownRope rope(std::vector<sf::Vector2f>{{200.f, 60.f}, {100.f, 60.f}}, level);
    for (int ii = 0; ii < static_cast<int>(3.f / DT); ++ii) {
        rope.update({100.f, 60.f}, DT, false);
        for (const sf::Vector2f& p : rope.points()) ASSERT_FALSE(inRock(*level, p)) << p.x << "," << p.y;
    }
    // Come to rest, with the hook lying on the floor or on the ledge.
    const std::vector<sf::Vector2f> before = rope.points();
    rope.update({100.f, 60.f}, DT, false);
    for (size_t ii = 0; ii < before.size(); ++ii) EXPECT_LT(distance(before[ii], rope.points()[ii]), 0.05f);
    const sf::Vector2f hook = rope.hook();
    EXPECT_TRUE(std::abs(hook.y - 8.f) < 1.f || std::abs(hook.y - 32.f) < 1.f) << hook.y;
}

TEST(ThrownRope, ReelingInTakesAsLongAsTheRopeIsLong) {
    auto level = Level::loadFromString(ROOM);
    // Lying along the floor and over the ledge: much longer than the straight line to the hook.
    const std::vector<sf::Vector2f> path = {{140.f, 32.5f}, {119.5f, 32.5f}, {119.5f, 8.5f}, {40.f, 8.5f}};
    ThrownRope rope(path, level);
    const float speed = 160.f;
    const float takes = rope.length() / speed;
    float time = 0.f;
    while (!rope.isIn() && time < 5.f) {
        rope.reelIn(speed * DT);
        rope.update({40.f, 13.f}, DT, false);
        for (const sf::Vector2f& p : rope.points()) ASSERT_FALSE(inRock(*level, p));
        time += DT;
    }
    EXPECT_TRUE(rope.isIn());
    EXPECT_NEAR(time, takes, 2.f * DT);
    EXPECT_GT(time, distance(path.front(), path.back()) / speed + 0.1f);
}

TEST(ThrownRope, PaysOutAsTheHookFlies) {
    auto level = Level::loadFromString(ROOM);
    const sf::Vector2f hand(40.f, 20.f);
    ThrownRope rope(hand, level);
    for (int ii = 1; ii <= 20; ++ii) {
        rope.flyTo(hand + sf::Vector2f(4.f * ii, 2.f * ii));
        rope.update(hand, DT, true);
    }
    EXPECT_EQ(rope.hook(), hand + sf::Vector2f(80.f, 40.f));
    EXPECT_NEAR(rope.length(), distance(hand, rope.hook()), ThrownRope::SEGMENT + 1.f);
}

TEST(ThrownRope, NeverFallsThroughAThinFloor) {
    // A shelf one tile thick (top at y = 48), wall to wall, with the hand above it: the rope is let
    // go high up and drops onto it hard.
    auto level = Level::loadFromString(
        "##############################\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "##############################\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#.S..........................#\n"
        "##############################\n");
    const sf::Vector2f hand(40.f, 60.f);
    ThrownRope rope(std::vector<sf::Vector2f>{{200.f, 140.f}, {120.f, 140.f}, hand}, level);
    for (int ii = 0; ii < static_cast<int>(3.f / DT); ++ii) {
        rope.update(hand, DT, false);
        for (const sf::Vector2f& p : rope.points()) ASSERT_GE(p.y, 48.f) << "fell through at " << p.x;
    }
}

TEST(ThrownRope, ReeledInFromBelowAShelfItSlidesOffTheEndNotThroughIt) {
    // A shelf one tile thick (y = 40 to 48) from the left wall to x = 200, then a gap. The rope lies
    // along the top of it, over its end and down to the hand underneath.
    auto level = Level::loadFromString(
        "##############################\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#........................#####\n"
        "########################.....#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#............................#\n"
        "#.S..........................#\n"
        "##############################\n");
    const float shelfTop = 56.f, shelfBottom = 48.f, gap = 192.f;
    const sf::Vector2f hand(40.f, 13.f);
    ThrownRope rope(std::vector<sf::Vector2f>{{60.f, shelfTop + 0.5f}, {gap + 4.f, shelfTop + 0.5f}, {gap + 4.f, 13.f}, hand}, level);
    std::vector<sf::Vector2f> before = rope.points();
    for (int ii = 0; ii < static_cast<int>(4.f / DT) && !rope.isIn(); ++ii) {
        rope.reelIn(120.f * DT);
        rope.update(hand, DT, false);
        // Counted from the hook, a point is the same point from step to step.
        const auto& now = rope.points();
        for (size_t p = 0; p + 1 < std::min(now.size(), before.size()); ++p) {
            const bool wentThrough = before[p].y >= shelfTop && now[p].y < shelfBottom && now[p].x < gap;
            ASSERT_FALSE(wentThrough) << "point " << p << " went through the shelf at x " << now[p].x;
        }
        before = now;
    }
}
