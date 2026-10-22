#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/Level.h"
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

    hold(player, moving(1.f), 0.1f);

    EXPECT_FLOAT_EQ(player.getVelocity().x, Player::MAX_RUN_SPEED);
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
    player.respawn({100.f, 48.f});

    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(60.f, 40.f);
    player.handleInput(in);
    ASSERT_NE(player.getRope(), nullptr);

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
    player.respawn({120.f, 8.f});

    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    in.moveY = 1.f;
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

    EXPECT_EQ(player.getRope(), nullptr);
    EXPECT_GE(player.getVelocity().y, Player::ROPE_JUMP_SPEED - 10.f);
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
    player.respawn({56.f, 54.f});

    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = {100.f, 88.f};
    player.handleInput(in);
    ASSERT_NE(player.getRope(), nullptr);
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

TEST(PlayerTest, LettingGoAndJumpingInTheSameFrameStillJumps) {
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
    EXPECT_GE(player.getVelocity().y, Player::ROPE_JUMP_SPEED - 10.f);
}

namespace
{
    // Hangs from the ceiling of ROOM, then jumps straight off the rope.
    void ropeJump(Player& player)
    {
        PlayerInput in;
        in.ropePressed = true;
        in.ropeHeld = true;
        in.aim = player.centre() + sf::Vector2f(0.f, 100.f);
        hold(player, in, DT);
        ASSERT_NE(player.getRope(), nullptr);

        in.ropePressed = false;
        in.jumpPressed = true;
        in.jumpHeld = true;
        hold(player, in, DT);
        ASSERT_EQ(player.getRope(), nullptr);
    }
}

TEST(PlayerTest, JumpingOffTheRopeWorksRightAway) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});

    ropeJump(player);
    EXPECT_GE(player.getVelocity().y, Player::ROPE_JUMP_SPEED - 10.f);
}

TEST(PlayerTest, RopeCantBeShotAgainRightAfterARopeJump) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});
    ropeJump(player);
    EXPECT_FALSE(player.canShootRope());

    PlayerInput shoot;
    shoot.ropePressed = true;
    shoot.ropeHeld = true;
    shoot.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    hold(player, shoot, DT);
    EXPECT_EQ(player.getRope(), nullptr);
}

TEST(PlayerTest, ClickDuringTheCooldownShootsWhenItEndsIfStillHeld) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});
    ropeJump(player);

    PlayerInput shoot;
    shoot.ropePressed = true;
    shoot.ropeHeld = true;
    shoot.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    hold(player, shoot, Player::ROPE_JUMP_COOLDOWN + 2.f * DT);
    EXPECT_NE(player.getRope(), nullptr);
}

TEST(PlayerTest, ClickDuringTheCooldownIsForgottenIfLetGo) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});
    ropeJump(player);

    PlayerInput click;
    click.ropePressed = true;
    click.ropeHeld = true;
    click.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    hold(player, click, DT);
    hold(player, PlayerInput{}, Player::ROPE_JUMP_COOLDOWN);
    EXPECT_EQ(player.getRope(), nullptr);
}

TEST(PlayerTest, LettingGoOfTheRopeHasAShorterCooldown) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});

    PlayerInput in;
    in.ropePressed = true;
    in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(0.f, 100.f);
    hold(player, in, 0.3f);
    hold(player, PlayerInput{}, DT);
    ASSERT_EQ(player.getRope(), nullptr);
    EXPECT_FALSE(player.canShootRope());

    hold(player, PlayerInput{}, Player::ROPE_COOLDOWN);
    EXPECT_TRUE(player.canShootRope());
}

TEST(PlayerTest, TheRopeCooldownCarriesOnAfterLanding) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({120.f, 50.f});
    ropeJump(player);
    player.setVelocity({0.f, -300.f}); // get back down quickly

    float waited = 0.f;
    while (!player.isGrounded() && waited < 1.f) {
        hold(player, PlayerInput{}, DT);
        waited += DT;
    }
    ASSERT_TRUE(player.isGrounded());
    ASSERT_LT(waited + DT, Player::ROPE_JUMP_COOLDOWN);
    hold(player, PlayerInput{}, DT);
    EXPECT_FALSE(player.canShootRope());

    hold(player, PlayerInput{}, Player::ROPE_JUMP_COOLDOWN - waited);
    EXPECT_TRUE(player.canShootRope());
}

TEST(PlayerTest, EverySkinHasARowOfFrames) {
    sf::Image sheet;
    ASSERT_TRUE(sheet.loadFromFile(config::assetPath("gfx/player.png")));
    // 10 frames of 12x14 per look, one look per row: the skins first (see tools/make_art.py).
    EXPECT_EQ(sheet.getSize().x, 10u * 12u);
    EXPECT_EQ(sheet.getSize().y, Player::LOOKS.size() * 14u);
    for (size_t ii = 0; ii < Player::SKINS.size(); ++ii) EXPECT_STREQ(Player::LOOKS[ii], Player::SKINS[ii]);
}

TEST(PlayerTest, EnteringAnotherRoomKeepsTheSpeedButLetsGoOfTheRope) {
    auto level = Level::loadFromString(ROOM);
    auto next = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({100.f, 48.f});
    PlayerInput in;
    in.ropePressed = in.ropeHeld = true;
    in.aim = player.centre() + sf::Vector2f(60.f, 40.f);
    player.handleInput(in);
    ASSERT_NE(player.getRope(), nullptr);
    player.setVelocity({150.f, -40.f});

    player.enterLevel(next, {-320.f, 8.f}, false);

    EXPECT_EQ(player.getPosition(), sf::Vector2f(-220.f, 56.f));
    EXPECT_EQ(player.getVelocity(), sf::Vector2f(150.f, -40.f));
    EXPECT_EQ(player.getRope(), nullptr);
}

TEST(PlayerTest, GoingUpIntoARoomGivesALift) {
    auto level = Level::loadFromString(ROOM);
    Player player(level);
    player.respawn({100.f, 48.f});
    player.setVelocity({0.f, 40.f});

    player.enterLevel(level, {0.f, -96.f}, true);

    EXPECT_FLOAT_EQ(player.getVelocity().y, Player::ROOM_ENTRY_LIFT);
}

TEST(PlayerTest, WithoutThePickTheRopeOnlySticksToMetal) {
    // Rock ceiling with two hook blocks in it, 6 tiles above the floor.
    auto level = Level::loadFromString(
        "################\n"
        "#.........%%...#\n"
        "#..............#\n"
        "#..............#\n"
        "#..............#\n"
        "#..............#\n"
        "#..S...........#\n"
        "################\n");
    Player player(level);
    player.setCanHookRock(false);
    const sf::Vector2f rock = player.centre() + sf::Vector2f(0.f, 60.f);
    const sf::Vector2f metal(84.f, 52.f);

    EXPECT_FALSE(player.ropeTarget(rock).has_value());
    ASSERT_TRUE(player.ropeBounce(rock).has_value());
    EXPECT_NEAR(player.ropeBounce(rock)->y, 56.f, 0.5f); // the underside of the ceiling
    EXPECT_TRUE(player.ropeTarget(metal).has_value());
    EXPECT_FALSE(player.ropeBounce(metal).has_value());

    PlayerInput in;
    in.ropePressed = in.ropeHeld = true;
    in.aim = rock;
    player.handleInput(in);
    EXPECT_EQ(player.getRope(), nullptr);
    auto events = player.takeEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, PlayerEvent::Type::RopeBounced);

    player.setCanHookRock(true);
    player.handleInput(in);
    EXPECT_NE(player.getRope(), nullptr);
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
    EXPECT_FLOAT_EQ(player.getVelocity().y, Player::AIR_JUMP_SPEED);
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
