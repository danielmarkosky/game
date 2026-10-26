#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/Level.h"
#include "src/Physics.h"
#include "src/Player.h"
#include "src/utils.h"
#include <cmath>
#include <string>

namespace
{
    // 40 x 12 room. Floor top is at y = 8, ceiling bottom at y = 88.
    // A ledge (top at y = 40) runs from the left wall to x = 80, and a pillar (top at y = 48)
    // starts at x = 224.
    const std::string ROOM =
        "########################################\n"
        "#......................................#\n"
        "#......................................#\n"
        "#......................................#\n"
        "#....S.................................#\n"
        "#......................................#\n"
        "#...........................#..........#\n"
        "##########..................#..........#\n"
        "#...........................#..........#\n"
        "#...........................#..........#\n"
        "#...........................#..........#\n"
        "########################################\n";

    constexpr float DT = config::FIXED_DT;

    // Feeds `in` for `seconds`. Button presses only count on the first frame, like real input.
    void hold(Player& player, PlayerInput in, float seconds)
    {
        int steps = static_cast<int>(std::round(seconds / DT));
        for (int ii = 0; ii < steps; ++ii) {
            player.handleInput(in);
            player.update(DT);
            in.jumpPressed = false;
            in.ropePressed = false;
        }
    }

    // Different physics for one test: put back as they were at the end of it.
    struct WithPhysics
    {
        explicit WithPhysics(const Physics& numbers) : before(physics()) { setPhysics(numbers); }
        ~WithPhysics() { setPhysics(before); }
        Physics before;
    };

    // Physics where jumping on the rope also kicks you upwards, as it used to.
    Physics ropeJumpKick()
    {
        Physics p;
        p.ropeJumpSpeed = 235.f;
        return p;
    }

    // Holds the rope's button aimed at `aim` until it catches: it has to fly there first.
    void catchRope(Player& player, sf::Vector2f aim)
    {
        PlayerInput in;
        in.ropePressed = true;
        in.ropeHeld = true;
        in.aim = aim;
        for (int ii = 0; ii < static_cast<int>(1.f / DT) && !player.getRope(); ++ii) {
            player.handleInput(in);
            player.update(DT);
            in.ropePressed = false;
        }
        ASSERT_NE(player.getRope(), nullptr);
    }

    PlayerInput moving(float x)
    {
        PlayerInput in;
        in.moveX = x;
        return in;
    }
}

TEST(PlayerTest, LandsExactlyOnTheFloor) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 70.f});

    hold(player, {}, 2.f);

    EXPECT_TRUE(player.isGrounded());
    EXPECT_FLOAT_EQ(player.getPosition().y, 8.f);
    EXPECT_FLOAT_EQ(player.getVelocity().y, 0.f);
}

TEST(PlayerTest, StopsFlushAgainstAWall) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({180.f, 8.f});

    hold(player, moving(1.f), 2.f);

    EXPECT_FLOAT_EQ(player.getPosition().x + Player::WIDTH, 224.f);
}

TEST(PlayerTest, HoldingADirectionKeepsSwingMomentum) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({40.f, 60.f});
    player.setVelocity({200.f, 0.f});

    hold(player, moving(1.f), 0.1f);

    // The old code snapped this straight down to the run speed (90).
    EXPECT_GT(player.getVelocity().x, 180.f);
}

TEST(PlayerTest, OverspeedOnGroundSlowsToRunSpeed) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 8.f});
    player.setVelocity({200.f, 0.f});

    // A slide back down to running, not a sudden stop.
    hold(player, moving(1.f), 0.1f);
    EXPECT_GT(player.getVelocity().x, physics().runSpeed + 50.f);
    hold(player, moving(1.f), 0.3f);
    EXPECT_FLOAT_EQ(player.getVelocity().x, physics().runSpeed);
}

TEST(PlayerTest, HigherJumpWhileHoldingJump) {
    auto level = Level::loadFromString(ROOM);

    auto peakHeight = [&](float holdTime) {
        Player player(level);
        player.respawn({120.f, 8.f});
        PlayerInput in;
        in.jumpPressed = true;
        in.jumpHeld = true;
        float peak = player.getPosition().y;
        for (int ii = 0; ii < static_cast<int>(1.f / DT); ++ii) {
            if (ii * DT >= holdTime) in = {};
            player.handleInput(in);
            player.update(DT);
            in.jumpPressed = false;
            peak = std::max(peak, player.getPosition().y);
        }
        return peak - 8.f;
    };

    float tap = peakHeight(DT);
    float full = peakHeight(0.5f);
    EXPECT_GT(full, 28.f);
    EXPECT_LT(tap, full * 0.6f);
}

TEST(PlayerTest, CoyoteTimeAllowsLateJump) {
    auto level = Level::loadFromString(ROOM);

    auto jumpAfterLeavingLedge = [&](float delay) {
        Player player(level);
        player.respawn({60.f, 40.f});
        while (player.isGrounded()) {
            player.handleInput(moving(1.f));
            player.update(DT);
        }
        hold(player, moving(1.f), delay);
        PlayerInput in = moving(1.f);
        in.jumpPressed = true;
        in.jumpHeld = true;
        hold(player, in, DT);
        return player.getVelocity().y;
    };

    EXPECT_GT(jumpAfterLeavingLedge(0.05f), 100.f);
    EXPECT_LT(jumpAfterLeavingLedge(0.2f), 0.f);
}

TEST(PlayerTest, JumpPressedJustBeforeLandingStillJumps) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 30.f});

    // Fall until a few pixels above the floor, then press jump while still in the air.
    while (player.getPosition().y > 11.f) {
        player.handleInput({});
        player.update(DT);
    }
    ASSERT_FALSE(player.isGrounded());

    PlayerInput in;
    in.jumpPressed = true;
    in.jumpHeld = true;
    hold(player, in, 0.08f);

    EXPECT_GT(player.getPosition().y, 10.f);
    EXPECT_GT(player.getVelocity().y, 0.f);
}

TEST(PlayerTest, RopeSwingStaysOnCircleAndKeepsMomentumAfterRelease) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.setRopeRange(100.f); // a long rope, for the room
    player.respawn({100.f, 48.f});

    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(60.f, 40.f);
    catchRope(player, in.aim);

    const sf::Vector2f anchor = player.getRope()->anchor();
    const float ropeLength = player.getRope()->length();
    EXPECT_NEAR(anchor.y, 88.f, 0.5f);

    in.ropePressed = false;
    float maxX = player.centre().x;
    float bottomSpeed = 0.f;
    for (int ii = 0; ii < static_cast<int>(1.2f / DT); ++ii) {
        player.handleInput(in);
        player.update(DT);
        EXPECT_LE(distance(player.centre(), anchor), ropeLength + 1.5f);
        maxX = std::max(maxX, player.centre().x);
        if (std::abs(player.centre().x - anchor.x) < 2.f) bottomSpeed = magnitude(player.getVelocity());
    }
    // Swung through the bottom and up the other side.
    EXPECT_GT(maxX, anchor.x + 20.f);
    EXPECT_GT(bottomSpeed, 120.f);

    // Let go mid-swing: the rope is gone and the player flies on.
    PlayerInput release;
    player.handleInput(release);
    EXPECT_EQ(player.getRope(), nullptr);
}

TEST(PlayerTest, ReelingInLiftsThePlayer) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.setRopeRange(100.f); // a long rope, for the room
    player.respawn({120.f, 8.f});

    PlayerInput in;
    in.ropeHeld = true;
    in.moveY = 1.f;
    catchRope(player, player.centre() + sf::Vector2f(0.f, 100.f));
    hold(player, in, 0.6f);

    ASSERT_NE(player.getRope(), nullptr);
    EXPECT_FALSE(player.isGrounded());
    EXPECT_GT(player.getPosition().y, 30.f);
}

TEST(PlayerTest, JumpingOffTheRopeLetsGo) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});

    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    hold(player, in, 0.3f);
    ASSERT_NE(player.getRope(), nullptr);
    ASSERT_FALSE(player.isGrounded());

    in.ropePressed = false;
    in.jumpPressed = true;
    in.jumpHeld = true;
    hold(player, in, DT);

    // Just let go: no speed from nowhere, the swing decides where you go.
    EXPECT_EQ(player.getRope(), nullptr);
    EXPECT_LE(player.getVelocity().y, 0.f);
}

TEST(PlayerTest, LettingGoOfTheRopeLeavesTheDoubleJump) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.setCanDoubleJump(true);
    player.respawn({120.f, 50.f});

    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    hold(player, in, 0.3f);
    PlayerInput jump = in; // still holding the rope's button
    jump.ropePressed = false;
    jump.jumpPressed = true;
    jump.jumpHeld = true;
    hold(player, jump, DT); // lets go
    ASSERT_EQ(player.getRope(), nullptr);
    EXPECT_TRUE(player.hasAirJump());
    hold(player, jump, DT); // and the feather
    EXPECT_FLOAT_EQ(player.getVelocity().y, physics().airJumpSpeed);
}

TEST(PlayerTest, RopeMissesWhenNothingIsInRange) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 8.f});

    // The left wall is ~115 px away, beyond the rope's range.
    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(-200.f, 0.f);
    player.handleInput(in);
    EXPECT_EQ(player.getRope(), nullptr);
}

TEST(PlayerTest, TouchingDanger) {
    auto level = Level::loadFromString(
        "..........\n"
        ".S........\n"
        "....^.....\n"
        "##########\n");
    Player player(level);

    EXPECT_FALSE(player.isTouchingDanger());
    bool touched = false;
    for (int ii = 0; ii < static_cast<int>(0.5f / DT) && !touched; ++ii) {
        player.handleInput(moving(1.f));
        player.update(DT);
        touched = player.isTouchingDanger();
    }
    EXPECT_TRUE(touched);
    // The spike is at x = 32..40; the hurtbox is 1px smaller than the player on each side.
    EXPECT_GT(player.getPosition().x + Player::WIDTH - 1.f, 32.f);
}

TEST(PlayerTest, OneWayPlatforms) {
    // Platform top at y = 32, floor top at y = 8.
    auto level = Level::loadFromString(
        "..........\n"
        "..........\n"
        "..........\n"
        "..------..\n"
        "..........\n"
        ".S........\n"
        "##########\n");
    Player player(level);
    player.respawn({24.f, 8.f});

    // Jump up through the platform from below and land on top of it.
    PlayerInput in;
    in.jumpPressed = true;
    in.jumpHeld = true;
    hold(player, in, 1.f);
    EXPECT_TRUE(player.isGrounded());
    EXPECT_FLOAT_EQ(player.getPosition().y, 32.f);

    // Holding down drops back through it.
    PlayerInput down;
    down.moveY = -1.f;
    hold(player, down, 1.f);
    EXPECT_FLOAT_EQ(player.getPosition().y, 8.f);
}

TEST(PlayerTest, RopeWrapsAroundABlockWhileSwinging) {
    // Ceiling bottom at y = 88, a block at x 128..152, y 56..64 hanging in the swing's path.
    auto level = Level::loadFromString(
        "########################################\n"
        "#......................................#\n"
        "#......................................#\n"
        "#......................................#\n"
        "#...............###....................#\n"
        "#......................................#\n"
        "#......................................#\n"
        "#......................................#\n"
        "#....S.................................#\n"
        "#......................................#\n"
        "#......................................#\n"
        "########################################\n");
    Player player(level);
    player.setRopeRange(100.f); // a long rope, for the room
    player.respawn({56.f, 54.f});

    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = {100.f, 88.f};
    catchRope(player, in.aim);
    in.ropePressed = false;

    size_t mostPivots = 1;
    for (int ii = 0; ii < static_cast<int>(2.f / DT); ++ii) {
        player.handleInput(in);
        player.update(DT);
        const Rope* rope = player.getRope();
        mostPivots = std::max(mostPivots, rope->pivots().size());
        // Never further from the last pivot than the rope that's left.
        EXPECT_LE(distance(player.centre(), rope->pivot()), rope->freeLength() + 1.5f);
    }
    EXPECT_GE(mostPivots, 2u);
}

TEST(PlayerTest, WithAKickLettingGoAndJumpingInTheSameFrameStillJumps) {
    const WithPhysics kick(ropeJumpKick());
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});

    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    hold(player, in, 0.3f);
    ASSERT_NE(player.getRope(), nullptr);

    PlayerInput letGoAndJump;
    letGoAndJump.jumpPressed = true;
    letGoAndJump.jumpHeld = true;
    hold(player, letGoAndJump, DT);

    EXPECT_EQ(player.getRope(), nullptr);
    EXPECT_GE(player.getVelocity().y, physics().ropeJumpSpeed - 10.f);
}

namespace
{
    // Hangs from the ceiling of ROOM, then jumps straight off the rope.
    void ropeJump(Player& player)
    {
        catchRope(player, player.centre() + sf::Vector2f(0.f, 100.f));
        PlayerInput in;
        in.ropeHeld = true;
        in.jumpPressed = true;
        in.jumpHeld = true;
        hold(player, in, DT);
        ASSERT_EQ(player.getRope(), nullptr);
    }
}

TEST(PlayerTest, WithAKickJumpingOffTheRopeWorksRightAway) {
    const WithPhysics kick(ropeJumpKick());
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});

    ropeJump(player);
    EXPECT_GE(player.getVelocity().y, physics().ropeJumpSpeed - 10.f);
}

TEST(PlayerTest, TheRopeFliesOutBeforeItCatches) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});
    const sf::Vector2f aim = player.centre() + sf::Vector2f(0.f, 100.f);
    const float away = 88.f - player.centre().y;

    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = aim;
    hold(player, in, DT);
    EXPECT_EQ(player.getRope(), nullptr); // on its way
    EXPECT_FALSE(player.canShootRope());
    float flown = DT;
    while (!player.getRope() && flown < 1.f) {
        hold(player, in, DT);
        flown += DT;
    }
    ASSERT_NE(player.getRope(), nullptr);
    EXPECT_NEAR(flown, away / physics().throwSpeed, 3.f * DT);
}

TEST(PlayerTest, TheRopeHasToComeBackBeforeItsThrownAgain) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});
    catchRope(player, player.centre() + sf::Vector2f(0.f, 100.f));
    hold(player, {}, DT); // let go
    ASSERT_EQ(player.getRope(), nullptr);
    EXPECT_FALSE(player.canShootRope());

    // It comes back from the hook to your hand, and you're falling away from it.
    hold(player, {}, 0.03f);
    EXPECT_FALSE(player.canShootRope());
    hold(player, {}, 0.15f);
    EXPECT_TRUE(player.canShootRope() || !player.isGrounded()); // in the air, and it's in your hand
}

TEST(PlayerTest, APressWhileItsComingBackThrowsAsSoonAsItsBack) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.setRopeRange(100.f); // the ceiling's still in reach once you've fallen to the floor
    player.respawn({120.f, 50.f});
    const sf::Vector2f up(0.f, 100.f);
    catchRope(player, player.centre() + up);
    hold(player, {}, DT);
    ASSERT_EQ(player.getRope(), nullptr);

    PlayerInput again;
    again.ropePressed = true;
    again.ropeHeld = true;
    again.aim = player.centre() + up;
    // Back, then out again: however long the rope takes both ways, at most.
    const Physics& p = physics();
    hold(player, again, 100.f / p.ropeReturnSpeed + 100.f / p.throwSpeed + 0.05f);
    EXPECT_NE(player.getRope(), nullptr);
}

TEST(PlayerTest, APressLetGoWhileItsComingBackIsForgotten) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});
    catchRope(player, player.centre() + sf::Vector2f(0.f, 100.f));
    hold(player, {}, DT);

    PlayerInput click;
    click.ropePressed = true;
    click.ropeHeld = true;
    click.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    hold(player, click, DT);
    hold(player, {}, 0.3f);
    EXPECT_EQ(player.getRope(), nullptr);
}

TEST(PlayerTest, AMissComesBackEmptyHanded) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 8.f});
    // Nothing within reach to the right: out as far as it goes, and back.
    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(100.f, 0.f);
    hold(player, in, DT);
    EXPECT_FALSE(player.canShootRope());
    in.ropePressed = false;
    const float roundTrip = player.reach() / physics().throwSpeed + player.reach() / physics().ropeReturnSpeed;
    hold(player, in, roundTrip + 3.f * DT);
    EXPECT_EQ(player.getRope(), nullptr);
    EXPECT_TRUE(player.canShootRope());
}

TEST(PlayerTest, AThrowInTheAirReachesLessFar) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 8.f});
    EXPECT_FLOAT_EQ(player.reach(), physics().ropeRange);
    player.respawn({120.f, 60.f});
    EXPECT_FLOAT_EQ(player.reach(), physics().ropeRange * physics().airThrowRange);
}

TEST(PlayerTest, UntilItCanBeThrownInTheAirItsSwungRoundStanding) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.setCanThrowInAir(false);
    player.respawn({120.f, 60.f});
    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    const auto thrown = [&] {
        for (const PlayerEvent& e : player.takeEvents()) {
            if (e.type == PlayerEvent::Type::RopeThrown) return true;
        }
        return false;
    };
    // In the air: not thrown.
    hold(player, in, 0.1f);
    EXPECT_FALSE(thrown());
    EXPECT_FALSE(player.canShootRope());

    // Standing: swung round first, then thrown.
    player.respawn({120.f, 8.f});
    hold(player, in, physics().throwWindup - 2.f * DT);
    EXPECT_FALSE(thrown());
    hold(player, in, 3.f * DT);
    EXPECT_TRUE(thrown());
}

TEST(PlayerTest, WithoutTheRopeTheresNothingToThrow) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.setHasRope(false);
    player.respawn({120.f, 50.f});
    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    hold(player, in, 0.5f);
    EXPECT_EQ(player.getRope(), nullptr);
    EXPECT_FALSE(player.canShootRope());
}

namespace
{
    // A wall to climb, 8 tiles high, its top at y = 72, with slick stone on its other side.
    // The floor's top is at y = 8; the wall's left face is at x = 120.
    const std::string WALL =
        "#####################\n"
        "#...................#\n"
        "#...................#\n"
        "#...................#\n"
        "#..............#o...#\n"
        "#..............#o...#\n"
        "#..............#o...#\n"
        "#..............#o...#\n"
        "#..............#o...#\n"
        "#..............#o...#\n"
        "#....S.........#o...#\n"
        "#..............#o...#\n"
        "#####################\n";

    // Jumps up against the wall's left face, holding towards it, until clinging.
    void grabTheWall(Player& player)
    {
        player.respawn({120.f - Player::WIDTH, 8.f});
        PlayerInput in;
        in.moveX = 1.f;
        in.jumpPressed = true;
        in.jumpHeld = true;
        for (int ii = 0; ii < static_cast<int>(1.f / DT) && !player.isClinging(); ++ii) {
            hold(player, in, DT);
            in.jumpPressed = false;
        }
        ASSERT_TRUE(player.isClinging());
    }

    PlayerInput climbing(float up)
    {
        PlayerInput in;
        in.moveX = 1.f; // into the wall
        in.moveY = up;
        return in;
    }
}

TEST(PlayerTest, WithTheChalkBagWallsCanBeClimbed) {
    auto level = Level::loadFromString(WALL);
    Player player(level);
    player.setCanClimb(true);
    grabTheWall(player);

    // Hanging there, not falling.
    const float y = player.getPosition().y;
    hold(player, climbing(0.f), 0.5f);
    EXPECT_NEAR(player.getPosition().y, y, 1.f);
    // Up.
    hold(player, climbing(1.f), 0.5f);
    EXPECT_NEAR(player.getPosition().y, y + physics().climbSpeed * 0.5f, 2.f);
    // Not holding towards it any more: letting go.
    hold(player, {}, DT);
    EXPECT_FALSE(player.isClinging());
}

TEST(PlayerTest, WithoutTheChalkBagWallsCantBeGrabbed) {
    auto level = Level::loadFromString(WALL);
    Player player(level);
    player.respawn({120.f - Player::WIDTH, 8.f});
    PlayerInput in;
    in.moveX = 1.f;
    in.jumpPressed = true;
    in.jumpHeld = true;
    hold(player, in, 1.f);
    EXPECT_FALSE(player.isClinging());
    EXPECT_TRUE(player.isGrounded());
}

TEST(PlayerTest, HoldingOnRunsOutOfStrength) {
    auto level = Level::loadFromString(WALL);
    Player player(level);
    player.setCanClimb(true);
    grabTheWall(player);
    hold(player, climbing(0.f), physics().climbStamina + 0.1f);
    EXPECT_EQ(player.staminaLeft(), 0.f);
    EXPECT_FLOAT_EQ(player.getVelocity().y, -physics().wallSlideSpeed);
    // Back on the ground it's all back.
    hold(player, climbing(0.f), 2.f);
    EXPECT_TRUE(player.isGrounded());
    EXPECT_FLOAT_EQ(player.staminaLeft(), 1.f);
}

TEST(PlayerTest, LeapingOffAWall) {
    auto level = Level::loadFromString(WALL);
    Player player(level);
    player.setCanClimb(true);
    grabTheWall(player);
    PlayerInput jump = climbing(0.f);
    jump.jumpPressed = true;
    jump.jumpHeld = true;
    hold(player, jump, DT);
    EXPECT_FALSE(player.isClinging());
    EXPECT_LT(player.getVelocity().x, -physics().wallJumpPush * 0.8f); // away from it
    EXPECT_GT(player.getVelocity().y, physics().jumpSpeed * 0.9f);
    // Still holding towards it, but it can't be grabbed again straight away.
    hold(player, climbing(0.f), Player::REGRAB_TIME * 0.5f);
    EXPECT_FALSE(player.isClinging());
}

TEST(PlayerTest, ClimbingOverTheTop) {
    auto level = Level::loadFromString(WALL);
    Player player(level);
    player.setCanClimb(true);
    grabTheWall(player);
    for (int ii = 0; ii < static_cast<int>(3.f / DT) && player.isClinging(); ++ii) hold(player, climbing(1.f), DT);
    // Still holding up and towards it: a hop up onto it.
    hold(player, climbing(1.f), 0.35f);
    hold(player, {}, 0.3f);
    EXPECT_TRUE(player.isGrounded());
    EXPECT_FLOAT_EQ(player.getPosition().y, 72.f); // standing on top of it
}

TEST(PlayerTest, SlickStoneCantBeGrabbed) {
    auto level = Level::loadFromString(WALL);
    Player player(level);
    player.setCanClimb(true);
    player.respawn({136.f, 8.f}); // right of the wall, by its slick side
    PlayerInput in;
    in.moveX = -1.f;
    in.jumpPressed = true;
    in.jumpHeld = true;
    hold(player, in, 1.f);
    EXPECT_FALSE(player.isClinging());
}

TEST(PlayerTest, EveryLookHasARowOfFrames) {
    sf::Image sheet;
    ASSERT_TRUE(sheet.loadFromFile(config::assetPath("gfx/player.png")));
    // 12 frames of 12x14 per look, one look per row: the heroine first (see tools/make_art.py).
    EXPECT_EQ(sheet.getSize().x, 12u * 12u);
    EXPECT_EQ(sheet.getSize().y, Player::LOOKS.size() * 14u);
    EXPECT_STREQ(Player::LOOKS[0], "heroine");
    EXPECT_EQ(Player::lookNamed("fen"), 2u);
    EXPECT_EQ(Player::lookNamed("nobody"), 0u);
}

TEST(PlayerTest, EnteringAnotherRoomKeepsTheSpeedButLetsGoOfTheRope) {
    auto level = Level::loadFromString(ROOM);
    auto next = Level::loadFromString(ROOM);
    Player player(level);
    player.setRopeRange(100.f); // a long rope, for the room
    player.respawn({100.f, 48.f});
    PlayerInput in;
    in.ropePressed = in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(60.f, 40.f);
    catchRope(player, in.aim);
    player.setVelocity({150.f, -40.f});
    const sf::Vector2f before = player.getPosition();

    player.enterLevel(next, {-320.f, 8.f}, false);

    EXPECT_EQ(player.getPosition(), before + sf::Vector2f(-320.f, 8.f));
    EXPECT_EQ(player.getVelocity(), sf::Vector2f(150.f, -40.f));
    EXPECT_EQ(player.getRope(), nullptr);
}

TEST(PlayerTest, GoingUpIntoARoomGivesALift) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({100.f, 48.f});
    player.setVelocity({0.f, 40.f});

    player.enterLevel(level, {0.f, -96.f}, true);

    EXPECT_FLOAT_EQ(player.getVelocity().y, physics().jumpSpeed);
}

TEST(PlayerTest, TheRopeSticksToRockAndHookBlocksButNotSlickStone) {
    // A ceiling 6 tiles above the floor: slick stone, then two hook blocks, then rock.
    auto level = Level::loadFromString(
        "################\n"
        "#oooooooo%%####\n"
        "#..............#\n"
        "#..............#\n"
        "#..............#\n"
        "#..............#\n"
        "#..S...........#\n"
        "################\n");
    Player player(level);
    player.setRopeRange(100.f);
    const sf::Vector2f slick = player.centre() + sf::Vector2f(0.f, 60.f);
    const sf::Vector2f metal(84.f, 52.f), rock(108.f, 52.f);

    EXPECT_FALSE(player.ropeTarget(slick).has_value());
    ASSERT_TRUE(player.ropeBounce(slick).has_value());
    EXPECT_NEAR(player.ropeBounce(slick)->y, 48.f, 0.5f); // the underside of the ceiling
    EXPECT_TRUE(player.ropeTarget(metal).has_value());
    EXPECT_TRUE(player.ropeTarget(rock).has_value());
    EXPECT_FALSE(player.ropeBounce(metal).has_value());

    // Thrown at slick stone: it flies there, bounces off and comes back.
    PlayerInput in;
    in.ropePressed = in.ropeHeld = true;
    in.aim = slick;
    hold(player, in, 0.3f);
    EXPECT_EQ(player.getRope(), nullptr);
    std::vector<PlayerEvent::Type> happened;
    for (const PlayerEvent& e : player.takeEvents()) happened.push_back(e.type);
    EXPECT_EQ(happened, (std::vector{PlayerEvent::Type::RopeThrown, PlayerEvent::Type::RopeBounced}));

    hold(player, {}, 1.f);
    catchRope(player, rock);
}

TEST(PlayerTest, DoubleJumpOnceUntilLanding) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 8.f});
    PlayerInput jump;
    jump.jumpPressed = jump.jumpHeld = true;

    // Without the feather, jumping in the air does nothing.
    hold(player, jump, 0.3f);
    const float fallingAt = player.getVelocity().y;
    hold(player, jump, DT);
    EXPECT_LE(player.getVelocity().y, fallingAt);
    hold(player, {}, 2.f);
    ASSERT_TRUE(player.isGrounded());

    player.setCanDoubleJump(true);
    hold(player, jump, 0.3f);
    EXPECT_TRUE(player.hasAirJump());
    hold(player, jump, DT);
    EXPECT_FLOAT_EQ(player.getVelocity().y, physics().airJumpSpeed);
    EXPECT_FALSE(player.hasAirJump());
    // Only once.
    hold(player, {}, 0.1f);
    const float before = player.getVelocity().y;
    hold(player, jump, DT);
    EXPECT_LT(player.getVelocity().y, before);
    // Back on landing.
    hold(player, {}, 2.f);
    EXPECT_TRUE(player.isGrounded());
    EXPECT_TRUE(player.hasAirJump());
}

TEST(PlayerTest, AStickAimsAtTheHookNearestWhereItPoints) {
    // A hook block up and to the right, steeper than the stick points, and slick stone to the left.
    auto level = Level::loadFromString(
        "########################################\n"
        "o......................................#\n"
        "o......................................#\n"
        "o........%.............................#\n"
        "o......................................#\n"
        "o......................................#\n"
        "o......................................#\n"
        "o......................................#\n"
        "o......................................#\n"
        "o......................................#\n"
        "o....S.................................#\n"
        "########################################\n");
    Player player(level);
    player.respawn({40.f, 8.f});

    // Straight up and to the right there's nothing; a little steeper there's the block.
    const sf::Vector2f up = player.centre() + sf::Vector2f(1.f, 1.f) * (physics().ropeRange / std::sqrt(2.f));
    EXPECT_FALSE(player.ropeTarget(up));
    const auto hit = player.ropeTarget(player.aimAlong({1.f, 1.f}));
    ASSERT_TRUE(hit);
    EXPECT_NEAR(hit->x, 76.f, 4.f);
    EXPECT_NEAR(hit->y, 64.f, 1.f);

    // To the left there's only slick stone: straight that way, as far as the rope goes.
    const sf::Vector2f left = player.aimAlong({-1.f, 0.f});
    EXPECT_FALSE(player.ropeTarget(left));
    EXPECT_NEAR(left.x, player.centre().x - physics().ropeRange, 0.01f);
    EXPECT_NEAR(left.y, player.centre().y, 0.01f);
}

TEST(PlayerTest, RunningTakesAMomentToStartAndToStop) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 8.f});
    const Physics& p = physics();

    hold(player, moving(1.f), 0.1f);
    EXPECT_GT(player.getVelocity().x, 0.f);
    EXPECT_LT(player.getVelocity().x, p.runSpeed * 0.6f);
    hold(player, moving(1.f), p.runSpeed / p.runAcceleration);
    EXPECT_FLOAT_EQ(player.getVelocity().x, p.runSpeed);

    // Letting go: a short slide, then still.
    const float x = player.getPosition().x;
    hold(player, {}, 0.05f);
    EXPECT_GT(player.getVelocity().x, 0.f);
    hold(player, {}, p.runSpeed / p.runDeceleration);
    EXPECT_FLOAT_EQ(player.getVelocity().x, 0.f);
    EXPECT_GT(player.getPosition().x - x, 3.f);
}

TEST(PlayerTest, PushingTheSwingCantMakeItFasterThanItsTopSpeed) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.setRopeRange(100.f); // a long rope, for the room
    player.respawn({120.f, 30.f});
    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    catchRope(player, in.aim);

    // Pushing the way it's swinging, each way in turn, for a long time.
    in.ropePressed = false;
    float fastest = 0.f;
    for (int ii = 0; ii < static_cast<int>(8.f / DT); ++ii) {
        in.moveX = player.getVelocity().x < 0.f ? -1.f : 1.f;
        hold(player, in, DT);
        fastest = std::max(fastest, magnitude(player.getVelocity()));
        // Never pushed round above the ceiling's hook.
        EXPECT_LT(player.centre().y, player.getRope()->pivot().y + 1.f);
    }
    EXPECT_GT(fastest, 100.f); // it does swing
    EXPECT_LE(fastest, physics().maxSwingSpeed + 1.f);
}

TEST(PlayerTest, AFlingSlowsDownInTheAir) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({40.f, 60.f});
    player.setVelocity({200.f, 100.f});
    hold(player, {}, 0.5f);
    EXPECT_NEAR(player.getVelocity().x, 200.f - physics().flingDeceleration * 0.5f, 2.f);
}

TEST(PlayerTest, LettingGoOfAWrappedRopeReelsItAllBackIn) {
    // Hooked over a block, swung round under it: let go, it comes back the long way round.
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.setRopeRange(100.f);
    player.respawn({120.f, 8.f});
    catchRope(player, {80.f, 88.f});
    PlayerInput in;
    in.ropeHeld = true;
    in.moveX = 1.f;
    hold(player, in, 1.f);
    ASSERT_NE(player.getRope(), nullptr);
    const std::vector<sf::Vector2f> path = player.getRope()->path();
    float length = 0.f;
    for (size_t ii = 1; ii < path.size(); ++ii) length += distance(path[ii - 1], path[ii]);

    hold(player, {}, DT); // let go
    float time = DT;
    while (!player.canShootRope() && time < 3.f) {
        hold(player, {}, DT);
        time += DT;
    }
    EXPECT_GE(time, length / physics().ropeReturnSpeed - 2.f * DT);
}

TEST(PlayerTest, ARopeThrownIntoAWallRightBesideYouBouncesOffIt) {
    // Standing against the left wall, throwing into it: rock too close to hook on, but not to hit.
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({8.f, 8.f});
    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(-50.f, 0.f);
    std::vector<PlayerEvent::Type> happened;
    for (int ii = 0; ii < static_cast<int>(1.f / DT); ++ii) {
        hold(player, in, DT);
        in.ropePressed = false;
        for (const PlayerEvent& e : player.takeEvents()) happened.push_back(e.type);
    }
    EXPECT_NE(std::find(happened.begin(), happened.end(), PlayerEvent::Type::RopeBounced), happened.end());
    EXPECT_EQ(player.getRope(), nullptr);
    EXPECT_TRUE(player.canShootRope()); // and back in your hand
}

namespace
{
    // A pool, water 5 tiles deep with its surface at y = 48, and dry ground on the right level with it.
    const std::string POOL =
        "########################\n"
        "#......................#\n"
        "#......................#\n"
        "#...S..................#\n"
        "#......................#\n"
        "#wwwwwwwwwwwwwww########\n"
        "#wwwwwwwwwwwwwww########\n"
        "#wwwwwwwwwwwwwww########\n"
        "#wwwwwwwwwwwwwww########\n"
        "#wwwwwwwwwwwwwww########\n"
        "########################\n";
    constexpr float SURFACE = 48.f;
}

TEST(PlayerTest, FallingInWaterYouFloatWithYourHeadOut) {
    auto level = Level::loadFromString(POOL);
    Player player(level);
    hold(player, {}, 3.f);
    EXPECT_TRUE(player.isSwimming());
    EXPECT_FALSE(player.isGrounded());
    // Feet under the surface, head above it, bobbing to a stop.
    EXPECT_NEAR(player.getPosition().y, SURFACE - Player::FLOAT_DEPTH, 2.f);
    EXPECT_LT(std::abs(player.getVelocity().y), 10.f);
}

TEST(PlayerTest, SwimmingAcrossYouStepOutOntoTheBank) {
    auto level = Level::loadFromString(POOL);
    Player player(level);
    hold(player, {}, 2.f);
    PlayerInput right;
    right.moveX = 1.f;
    hold(player, right, 0.5f);
    EXPECT_NEAR(std::abs(player.getVelocity().x), physics().swimSpeed, 1.f); // slower than running
    hold(player, right, 4.f);
    EXPECT_FALSE(player.isSwimming());
    EXPECT_TRUE(player.isGrounded());
    EXPECT_EQ(player.getPosition().y, SURFACE);
    EXPECT_GE(player.getPosition().x, 128.f);
}

TEST(PlayerTest, DivingDownAndBobbingBackUp) {
    auto level = Level::loadFromString(POOL);
    Player player(level);
    hold(player, {}, 2.f);
    PlayerInput down;
    down.moveY = -1.f;
    hold(player, down, 0.6f);
    EXPECT_LT(player.getPosition().y, SURFACE - 20.f);
    hold(player, {}, 3.f);
    EXPECT_NEAR(player.getPosition().y, SURFACE - Player::FLOAT_DEPTH, 2.f);
}

TEST(PlayerTest, JumpingAtTheSurfaceLeapsOutOfTheWater) {
    auto level = Level::loadFromString(POOL);
    Player player(level);
    hold(player, {}, 3.f);
    PlayerInput jump;
    jump.jumpPressed = jump.jumpHeld = true;
    float top = player.getPosition().y;
    for (int i = 0; i < static_cast<int>(0.6f / DT); ++i) {
        hold(player, jump, DT);
        jump.jumpPressed = false;
        top = std::max(top, player.getPosition().y);
    }
    EXPECT_GT(top, SURFACE + 12.f); // feet well clear of it: high enough to land on a ledge at the edge
    bool splashed = false;
    for (const PlayerEvent& e : player.takeEvents()) splashed |= e.type == PlayerEvent::Type::Splashed;
    EXPECT_TRUE(splashed);
}

TEST(PlayerTest, UnderWaterYouHoldYourBreathAndDrownWhenItRunsOut) {
    auto level = Level::loadFromString(POOL);
    Player player(level);
    // Floating, head out: breathing.
    hold(player, {}, 3.f);
    EXPECT_FALSE(player.isUnderwater());
    EXPECT_FLOAT_EQ(player.airLeft(), 1.f);
    PlayerInput down;
    down.moveY = -1.f;
    hold(player, down, physics().airTime * 0.5f);
    EXPECT_TRUE(player.isUnderwater());
    EXPECT_GT(player.airLeft(), 0.3f);
    EXPECT_LT(player.airLeft(), 0.7f);
    EXPECT_FALSE(player.isOutOfAir());
    hold(player, down, physics().airTime * 0.5f + 0.2f);
    EXPECT_TRUE(player.isOutOfAir());
}

TEST(PlayerTest, ComingUpForAirFillsYourLungsAgain) {
    auto level = Level::loadFromString(POOL);
    Player player(level);
    hold(player, {}, 2.f);
    PlayerInput down;
    down.moveY = -1.f;
    hold(player, down, physics().airTime * 0.75f);
    bool bubbled = false;
    for (const PlayerEvent& e : player.takeEvents()) bubbled |= e.type == PlayerEvent::Type::Bubbled;
    EXPECT_TRUE(bubbled);
    // Let go: you bob up, gasp, and get your breath back.
    hold(player, {}, physics().airRefillTime + 2.f);
    bool gasped = false;
    for (const PlayerEvent& e : player.takeEvents()) gasped |= e.type == PlayerEvent::Type::Gasped;
    EXPECT_TRUE(gasped);
    EXPECT_FALSE(player.isUnderwater());
    EXPECT_FLOAT_EQ(player.airLeft(), 1.f);
}
