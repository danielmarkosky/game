#include <gtest/gtest.h>
#include <algorithm>
#include "src/Config.h"
#include "src/Level.h"
#include "src/Physics.h"
#include "src/Player.h"

// The things in rooms that move or change: ice, crumbling blocks, clouds, icicles, updrafts,
// wind, hook rings, and what the rope tugs (levers, lamps).
namespace
{
    constexpr float DT = config::FIXED_DT;

    // Steps the room and the player together, like the game does.
    void run(Level& level, Player& player, PlayerInput in, float seconds)
    {
        for (int ii = 0; ii < static_cast<int>(seconds / DT); ++ii) {
            player.handleInput(in);
            level.update(DT, player.hitbox());
            player.update(DT);
            in.jumpPressed = in.ropePressed = false;
        }
    }

    PlayerInput moving(float x)
    {
        PlayerInput in;
        in.moveX = x;
        return in;
    }

    std::vector<PlayerEvent::Type> eventsOf(Player& player)
    {
        std::vector<PlayerEvent::Type> types;
        for (const PlayerEvent& e : player.takeEvents()) types.push_back(e.type);
        return types;
    }

    bool has(const std::vector<PlayerEvent::Type>& types, PlayerEvent::Type type)
    {
        return std::find(types.begin(), types.end(), type) != types.end();
    }
}

TEST(Mechanics, IceIsSlowToStopOn) {
    // The same run, let go of at full speed: on rock, then on ice.
    const auto slide = [](const std::string& floor) {
        auto level = Level::loadFromString("#" + std::string(60, '.') + "#\n#.S" + std::string(58, '.') + "#\n" + std::string(62, floor[0]) + "\n");
        Player player(*&level);
        run(*level, player, moving(1.f), 1.f);
        const float x = player.getPosition().x;
        run(*level, player, {}, 3.f);
        return player.getPosition().x - x;
    };
    EXPECT_GT(slide("~"), 4.f * slide("#"));
}

TEST(Mechanics, CrumblingBlocksGiveWayAndComeBack) {
    auto level = Level::loadFromString(
        "########\n"
        "#......#\n"
        "#..S...#\n"
        "#.&&&..#\n"
        "#......#\n"
        "#......#\n"
        "########\n");
    Player player(level);
    player.respawn({24.f, 32.f}); // on the crumbling blocks
    run(*level, player, {}, 0.2f);
    EXPECT_TRUE(player.isGrounded());
    run(*level, player, {}, 0.6f);
    EXPECT_LT(player.getPosition().y, 32.f); // fallen through
    run(*level, player, {}, 2.5f);
    EXPECT_TRUE(level->isSolid(3, 3));        // back
}

TEST(Mechanics, CrumblingCanBeHeard) {
    auto level = Level::loadFromString(
        "########\n"
        "#......#\n"
        "#..S...#\n"
        "#.&&&..#\n"
        "#......#\n"
        "#......#\n"
        "########\n");
    Player player(level);
    player.respawn({24.f, 32.f});
    std::vector<LevelSound::Kind> heard;
    for (int i = 0; i < 60; ++i) {
        level->update(1.f / 60.f, player.hitbox());
        player.update(1.f / 60.f);
        for (const LevelSound& sound : level->takeSounds()) heard.push_back(sound.kind);
    }
    // A crack when it's stood on, then the crumbling: once each per block, nothing more.
    ASSERT_FALSE(heard.empty());
    EXPECT_EQ(heard.front(), LevelSound::Kind::Crack);
    EXPECT_TRUE(std::count(heard.begin(), heard.end(), LevelSound::Kind::Crumble) >= 1);
    EXPECT_LE(heard.size(), 6u);
    EXPECT_TRUE(level->takeSounds().empty());
}

TEST(Mechanics, CloudsFadeFromUnderYou) {
    auto level = Level::loadFromString(
        "########\n"
        "#......#\n"
        "#..S...#\n"
        "#.___..#\n"
        "#......#\n"
        "#......#\n"
        "########\n");
    Player player(level);
    player.respawn({24.f, 32.f});
    run(*level, player, {}, 0.4f);
    EXPECT_TRUE(player.isGrounded());
    run(*level, player, {}, 0.8f);
    EXPECT_LT(player.getPosition().y, 32.f);
}

TEST(Mechanics, AnIcicleFallsOnYouWhenYouGoUnderIt) {
    auto level = Level::loadFromString(
        "################\n"
        "#.........Y....#\n"
        "#..............#\n"
        "#..............#\n"
        "#..............#\n"
        "#..S...........#\n"
        "################\n");
    Player player(level);
    ASSERT_EQ(level->icicles().size(), 1u);
    // Standing under it: it shakes, falls, and that's the end of you.
    player.respawn({80.f, 8.f});
    bool hit = false;
    for (int ii = 0; ii < static_cast<int>(1.5f / DT) && !hit; ++ii) {
        level->update(DT, player.hitbox());
        player.update(DT);
        hit = player.isTouchingDanger();
    }
    EXPECT_TRUE(hit);
    // Away from it, it hangs there.
    level->reset();
    player.respawn({24.f, 8.f});
    run(*level, player, {}, 1.f);
    EXPECT_EQ(level->icicles()[0].state, Icicle::State::Hanging);
}

TEST(Mechanics, UpdraftsLiftYou) {
    auto level = Level::loadFromString(
        "############\n"
        "#..........#\n"
        "#..........#\n"
        "#..........#\n"
        "#..........#\n"
        "#..........#\n"
        "#..........#\n"
        "#.S...U....#\n"
        "############\n");
    Player player(level);
    player.respawn({48.f, 20.f}); // in it
    run(*level, player, {}, 0.5f);
    EXPECT_GT(player.getPosition().y, 40.f);
    EXPECT_GT(player.getVelocity().y, 0.f);
}

TEST(Mechanics, WindPushesWhatsInTheAir) {
    const auto drift = [](float wind) {
        auto level = Level::loadFromString("#" + std::string(60, '.') + "#\n#" + std::string(60, '.') + "#\n#..S" + std::string(57, '.') + "#\n" + std::string(62, '#') + "\n");
        level->setWind(wind);
        Player player(level);
        player.respawn({240.f, 8.f});
        PlayerInput jump;
        jump.jumpPressed = jump.jumpHeld = true;
        run(*level, player, jump, 0.5f);
        return player.getPosition().x - 240.f;
    };
    EXPECT_GT(drift(100.f), 4.f);
    EXPECT_LT(drift(-100.f), -4.f);
    EXPECT_EQ(drift(0.f), 0.f);
}

TEST(Mechanics, ARingCarriesTheRopeAlong) {
    auto level = Level::loadFromString(
        "################################\n"
        "#..............................#\n"
        "#..............................#\n"
        "#..............................#\n"
        "#.....R........................#\n"
        "#..............................#\n"
        "#..............................#\n"
        "#..............................#\n"
        "#..S...........................#\n"
        "################################\n");
    ASSERT_EQ(level->rings().size(), 1u);
    Player player(level);
    player.setRopeRange(100.f);
    PlayerInput in;
    in.ropePressed = in.ropeHeld = true;
    in.aim = level->ringPosition(0);
    for (int ii = 0; ii < static_cast<int>(1.f / DT) && !player.getRope(); ++ii) {
        in.aim = level->ringPosition(0);
        player.handleInput(in);
        level->update(DT, player.hitbox());
        player.update(DT);
        in.ropePressed = false;
    }
    ASSERT_NE(player.getRope(), nullptr);
    run(*level, player, in, 1.f);
    EXPECT_NEAR(player.getRope()->anchor().x, level->ringPosition(0).x, 0.01f);
    EXPECT_GT(player.getRope()->anchor().x, level->rings()[0].from.x + 4.f); // and it's moved on
}

TEST(Mechanics, TheRopeTugsLeversAndLampsAndComesBack) {
    auto level = Level::loadFromString(
        "################\n"
        "#..............#\n"
        "#........Q.....#\n"
        "#..............#\n"
        "#..............#\n"
        "#..S......L....#\n"
        "################\n");
    for (const auto& [target, type] : {std::pair{sf::Vector2f(84.f, 12.f), PlayerEvent::Type::LeverTugged},
                                       std::pair{sf::Vector2f(76.f, 36.f), PlayerEvent::Type::LampTugged}}) {
        Player player(level);
        player.respawn({24.f, 8.f});
        player.setRopeRange(100.f);
        ASSERT_TRUE(player.aimHit(target));
        PlayerInput in;
        in.ropePressed = in.ropeHeld = true;
        in.aim = target;
        std::vector<PlayerEvent::Type> happened;
        for (int ii = 0; ii < static_cast<int>(2.f / DT); ++ii) {
            player.handleInput(in);
            level->update(DT, player.hitbox());
            player.update(DT);
            in.ropePressed = false;
            for (auto t : eventsOf(player)) happened.push_back(t);
        }
        EXPECT_TRUE(has(happened, type));
        EXPECT_EQ(player.getRope(), nullptr);
        EXPECT_TRUE(player.canShootRope()); // back in the hand
    }
}

TEST(Mechanics, HookedInACrumblingBlockYouFallWhenItGoes) {
    auto level = Level::loadFromString(
        "################\n"
        "#.....&&&......#\n"
        "#..............#\n"
        "#..............#\n"
        "#..............#\n"
        "#..S...........#\n"
        "################\n");
    Player player(level);
    player.respawn({52.f, 8.f});
    PlayerInput in;
    in.ropePressed = in.ropeHeld = true;
    in.aim = {56.f, 60.f};
    for (int ii = 0; ii < static_cast<int>(1.f / DT) && !player.getRope(); ++ii) {
        player.handleInput(in);
        player.update(DT);
        in.ropePressed = false;
    }
    ASSERT_NE(player.getRope(), nullptr);
    // Crumbled by standing on it (from above, as if): the rope comes loose.
    for (int ii = 0; ii < static_cast<int>(1.f / DT); ++ii) level->update(DT, {48.f, 48.f, 24.f, 11.f});
    ASSERT_FALSE(level->isSolid(6, 5));
    ASSERT_FALSE(level->isSolid(7, 5));
    run(*level, player, in, 0.1f);
    EXPECT_EQ(player.getRope(), nullptr);
}

TEST(Mechanics, IceCantBeGrabbed) {
    auto level = Level::loadFromString(
        "################\n"
        "#.........~....#\n"
        "#.........~....#\n"
        "#.........~....#\n"
        "#.........~....#\n"
        "#..S......~....#\n"
        "################\n");
    Player player(level);
    player.setCanClimb(true);
    player.respawn({73.f, 8.f}); // against it
    PlayerInput in;
    in.moveX = 1.f;
    in.jumpPressed = in.jumpHeld = true;
    run(*level, player, in, 1.f);
    EXPECT_FALSE(player.isClinging());
}
