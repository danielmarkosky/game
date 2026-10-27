#include <gtest/gtest.h>
#include <algorithm>
#include "src/Config.h"
#include "src/Dog.h"
#include "src/Level.h"
#include "src/Physics.h"
#include "src/Player.h"
#include "src/utils.h"
#include "src/World.h"

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

TEST(Mechanics, ABounceMushroomThrowsYouUpHigherThanAJump) {
    // Standing on mushrooms: thrown up, over and over, higher than a jump goes; holding down, you stay.
    std::string rows;
    for (int i = 0; i < 20; ++i) rows += "#" + std::string(20, '.') + "#\n";
    auto level = Level::loadFromString(rows + "#....S...............#\n#mmmmmmmmmmmmmmmmmmmm#\n" + std::string(22, '#') + "\n");
    Player player(level);
    const float floor = player.getPosition().y;
    float top = floor;
    for (int ii = 0; ii < static_cast<int>(1.f / DT); ++ii) {
        run(*level, player, {}, DT);
        top = std::max(top, player.getPosition().y);
    }
    const float jump = physics().jumpSpeed * physics().jumpSpeed / (2.f * physics().gravity);
    EXPECT_GT(top - floor, jump * 1.5f);
    EXPECT_TRUE(has(eventsOf(player), PlayerEvent::Type::Bounced));
    // Holding down: standing on it.
    PlayerInput down;
    down.moveY = -1.f;
    run(*level, player, down, 2.f);
    EXPECT_TRUE(player.isGrounded());
    EXPECT_EQ(player.getPosition().y, floor);
}

TEST(Mechanics, AHiddenPassageIsNotThere) {
    // '?' looks like rock, but you walk straight through it, and the rope doesn't bite it.
    auto level = Level::loadFromString("##########\n#........#\n#.S...??.#\n##########\n");
    Player player(level);
    run(*level, player, moving(1.f), 1.5f);
    EXPECT_GT(player.getPosition().x, 7.f * Level::TILE_SIZE);
    EXPECT_FALSE(level->ropeHit({20.f, 12.f}, {60.f, 12.f}, 50.f).has_value());
}

namespace
{
    // A room with no gravity, open in the middle.
    std::shared_ptr<Level> spaceRoom()
    {
        auto level = Level::loadFromString("####################\n#..................#\n#..................#\n#..................#\n"
                                           "#.S................#\n#..................#\n#..................#\n####################\n");
        level->setSpace(true);
        return level;
    }

    // Throws the rope at `aim` and holds it until it's caught (or a second's gone by).
    void hookAt(Level& level, Player& player, sf::Vector2f aim)
    {
        PlayerInput in;
        in.ropePressed = in.ropeHeld = true;
        in.aim = aim;
        for (int ii = 0; ii < static_cast<int>(1.f / DT) && !player.getRope(); ++ii) {
            player.handleInput(in);
            level.update(DT, player.hitbox());
            player.update(DT);
            in.ropePressed = false;
        }
    }
}

TEST(Mechanics, WithNoGravityTheArrowsDontMoveYouOutInTheOpen) {
    auto level = spaceRoom();
    Player player(level);
    player.respawn({60.f, 24.f});
    run(*level, player, {}, 1.f);
    EXPECT_FLOAT_EQ(player.getPosition().y, 24.f); // nothing pulls you down
    PlayerInput push;
    push.moveX = push.moveY = 1.f;
    run(*level, player, push, 0.5f);
    EXPECT_FLOAT_EQ(player.getPosition().x, 60.f); // and nothing to push off
    EXPECT_FLOAT_EQ(player.getPosition().y, 24.f);
    EXPECT_TRUE(player.isFloating());
    EXPECT_FALSE(player.holdingOn());
}

TEST(Mechanics, WithNoGravityYouClamberAlongRockButNeverOffIt) {
    auto level = spaceRoom();
    Player player(level);
    player.respawn({8.f, 24.f}); // against the left wall
    run(*level, player, {}, 0.2f);
    EXPECT_TRUE(player.holdingOn());
    PlayerInput up;
    up.moveY = 1.f;
    run(*level, player, up, 0.5f); // up along it
    EXPECT_NEAR(player.getPosition().y, 24.f + physics().floatCrawl * 0.5f, 2.f);
    EXPECT_FLOAT_EQ(player.getPosition().x, 8.f);
    run(*level, player, moving(1.f), 0.5f); // away from it: no
    EXPECT_FLOAT_EQ(player.getPosition().x, 8.f);
    EXPECT_TRUE(player.holdingOn());
}

TEST(Mechanics, WithNoGravityThrowingTheRopePushesYouBack) {
    auto level = spaceRoom();
    Player player(level);
    player.respawn({60.f, 24.f});
    PlayerInput in;
    in.ropePressed = in.ropeHeld = true;
    in.aim = {150.f, 30.f};
    run(*level, player, in, DT * 2.f);
    EXPECT_LT(player.getVelocity().x, -physics().throwRecoil * 0.8f);
}

TEST(Mechanics, WithNoGravityReelingInPullsYouAlongTheRopeAndYouDriftOnAfter) {
    auto level = spaceRoom();
    Player player(level);
    player.respawn({60.f, 24.f});
    player.setRopeRange(100.f);
    hookAt(*level, player, {152.f, 30.f}); // the right wall
    ASSERT_NE(player.getRope(), nullptr);
    PlayerInput reel;
    reel.ropeHeld = true;
    reel.moveY = 1.f;
    run(*level, player, reel, 0.4f);
    EXPECT_GT(player.getVelocity().x, 80.f);
    const float speed = player.getVelocity().x;
    run(*level, player, {}, 0.2f); // let go of the rope
    EXPECT_EQ(player.getRope(), nullptr);
    EXPECT_NEAR(player.getVelocity().x, speed, speed * 0.1f);
}

TEST(Mechanics, WithNoGravityTheRopeHeldSwingsYouRound) {
    // Drifting past where it's hooked, the rope catches you and turns you: you go round, not on.
    auto level = spaceRoom();
    Player player(level);
    player.respawn({60.f, 24.f});
    hookAt(*level, player, {60.f, 56.f}); // the roof, straight up
    ASSERT_NE(player.getRope(), nullptr);
    player.setVelocity({60.f, 0.f});
    PlayerInput hold;
    hold.ropeHeld = true;
    run(*level, player, hold, 0.5f);
    EXPECT_GT(player.getVelocity().y, 10.f); // swung up towards the roof
    EXPECT_LT(player.getPosition().x, 60.f + 60.f * 0.5f);
}

TEST(Mechanics, ACartRollsOnUntilAWallWrecksIt) {
    auto level = Level::loadFromString("#################\n#...............#\n#S..............#\nttttttttttttttt.#\n#################\n");
    Player player(level);
    run(*level, player, {}, 0.5f);
    player.startRiding(1);
    run(*level, player, moving(-1.f), 0.5f); // holding back does nothing
    EXPECT_NEAR(player.getVelocity().x, physics().cartSpeed, 1.f);
    EXPECT_GT(player.getPosition().x, 40.f);
    run(*level, player, {}, 2.f);
    EXPECT_TRUE(player.hasCrashed());
}

TEST(Mechanics, TheDivingSuitHoldsMoreBreath) {
    const auto drownsAfter = [](float scale) {
        // Held down on the bottom of a pool.
        auto level = Level::loadFromString("#######\n#S....#\n##www##\n##www##\n##www##\n#######\n");
        Player player(level);
        player.setBreathScale(scale);
        player.respawn({24.f, 8.f});
        PlayerInput down;
        down.moveY = -1.f;
        float t = 0.f;
        while (!player.isOutOfAir() && t < 60.f) {
            run(*level, player, down, DT);
            t += DT;
        }
        return t;
    };
    EXPECT_NEAR(drownsAfter(physics().divingAir), physics().divingAir * drownsAfter(1.f), 0.5f);
}


TEST(Mechanics, PipFollowsTheWayYouWent) {
    // He keeps a little way behind along your path, and stays put when you stop.
    Dog dog;
    dog.reset({10.f, 8.f});
    sf::Vector2f you(10.f, 8.f);
    for (int i = 0; i < 120; ++i) {
        you.x += 1.f;
        if (i > 40 && i < 60) you.y += 1.f; // up a step
        dog.follow(you, DT);
    }
    EXPECT_LT(dog.feet().x, you.x);
    EXPECT_NEAR(you.x - dog.feet().x, Dog::BEHIND, 2.f);
    EXPECT_FLOAT_EQ(dog.feet().y, you.y); // up the step behind you
    const sf::Vector2f stopped = dog.feet();
    for (int i = 0; i < 60; ++i) dog.follow(you, DT);
    EXPECT_EQ(dog.feet(), stopped);
    dog.point(you + sf::Vector2f(40.f, 0.f));
    EXPECT_TRUE(dog.isPointing());
}

TEST(Mechanics, ReeledInStandingTheRopeDragsAStone) {
    auto level = Level::loadFromString(
        "####################\n"
        "#..................#\n"
        "#..................#\n"
        "#..S.........O.....#\n"
        "####################\n");
    Player player(level);
    run(*level, player, {}, 0.2f);
    const float startX = player.getPosition().x;
    const float stoneX = level->stones()[0].position.x;
    player.setRopeRange(100.f);
    hookAt(*level, player, level->stones()[0].position + sf::Vector2f(8.f, 8.f));
    ASSERT_NE(player.getRope(), nullptr);
    PlayerInput reel;
    reel.ropeHeld = true;
    reel.moveY = 1.f;
    run(*level, player, reel, 1.f);
    EXPECT_LT(level->stones()[0].position.x, stoneX - physics().stoneDragSpeed * 0.8f); // it comes...
    EXPECT_NEAR(player.getPosition().x, startX, 1.f);                                   // ...you don't
    EXPECT_FLOAT_EQ(level->stones()[0].position.y, 8.f);                                // along the floor
    level->reset();
    EXPECT_FLOAT_EQ(level->stones()[0].position.x, stoneX); // and back where it was
}

TEST(Mechanics, AStoneIsSolidAndFalls) {
    auto level = Level::loadFromString(
        "##########\n"
        "#........#\n"
        "#....O...#\n"
        "#........#\n"
        "#........#\n"
        "#.S......#\n"
        "##########\n");
    Player player(level);
    run(*level, player, {}, 1.f);
    EXPECT_FLOAT_EQ(level->stones()[0].position.y, 8.f); // fell to the floor
    EXPECT_FALSE(level->isEmptySpace({44.f, 8.f}, 4.f, 4.f));
    run(*level, player, moving(1.f), 1.f);
    EXPECT_LE(player.getPosition().x + Player::WIDTH, level->stones()[0].position.x); // stopped by it
}

TEST(Mechanics, AStoneOnAWeightPlateHoldsTheGatesOpenAndYouDont) {
    const std::string stone =
        "##########\n"
        "#........#\n"
        "#........#\n"
        "#.O......#\n"
        "#......=.#\n"
        "#......=.#\n"
        "#.V....=S#\n"
        "##########\n";
    auto level = Level::loadFromString(stone);
    Player player(level);
    EXPECT_TRUE(level->isSolid(7, 2));
    run(*level, player, {}, 1.f); // the stone drops onto the plate
    EXPECT_TRUE(level->plates()[0].down);
    EXPECT_FALSE(level->isSolid(7, 2));
    // Without the stone, standing on the plate does nothing.
    std::string empty = stone;
    empty[empty.find('O')] = '.';
    auto bare = Level::loadFromString(empty);
    Player you(bare);
    you.respawn({16.f, 8.f});
    run(*bare, you, {}, 0.5f);
    EXPECT_FALSE(bare->plates()[0].down);
    EXPECT_TRUE(bare->isSolid(7, 2));
}

TEST(Mechanics, FireBurnsTheRopeThrough) {
    auto level = Level::loadFromString(
        "####################\n"
        "#..................#\n"
        "#..................#\n"
        "#..................#\n"
        "#..S.....F.........#\n"
        "####################\n");
    Player player(level);
    player.setRopeRange(100.f);
    run(*level, player, {}, 0.2f);
    // Thrown through it: burnt off on the way, and back.
    const sf::Vector2f through(76.f, 12.f);
    ASSERT_TRUE(player.aimHit(through));
    EXPECT_EQ(player.aimHit(through)->kind, RopeHit::Kind::Fire);
    EXPECT_FALSE(player.ropeTarget(through));
    PlayerInput in;
    in.ropePressed = in.ropeHeld = true;
    in.aim = through;
    std::vector<PlayerEvent::Type> happened;
    for (int ii = 0; ii < static_cast<int>(1.f / DT); ++ii) {
        player.handleInput(in);
        level->update(DT, player.hitbox());
        player.update(DT);
        in.ropePressed = false;
        for (auto t : eventsOf(player)) happened.push_back(t);
    }
    EXPECT_TRUE(has(happened, PlayerEvent::Type::RopeBurnt));
    EXPECT_EQ(player.getRope(), nullptr);
    // Hooked over it and let out: the slack rope sags into it.
    run(*level, player, {}, 2.f);
    hookAt(*level, player, {100.f, 40.f}); // the roof, beyond it
    ASSERT_NE(player.getRope(), nullptr);
    PlayerInput out;
    out.ropeHeld = true;
    out.moveY = -1.f;
    happened.clear();
    for (int ii = 0; ii < static_cast<int>(2.f / DT) && player.getRope(); ++ii) {
        player.handleInput(out);
        level->update(DT, player.hitbox());
        player.update(DT);
        for (auto t : eventsOf(player)) happened.push_back(t);
    }
    EXPECT_TRUE(has(happened, PlayerEvent::Type::RopeBurnt));
    EXPECT_EQ(player.getRope(), nullptr);
}

TEST(Mechanics, FireBurnsYou) {
    auto level = Level::loadFromString("##########\n#........#\n#.S..F...#\n##########\n");
    Player player(level);
    bool burnt = false;
    for (int ii = 0; ii < static_cast<int>(1.f / DT) && !burnt; ++ii) {
        run(*level, player, moving(1.f), DT);
        burnt = player.isTouchingDanger();
    }
    EXPECT_TRUE(burnt);
}

namespace
{
    // A ledge on the left to stand on, a long drop, and hook blocks low in a wall across it.
    const char* const ACROSS =
        "########################\n"
        "#......................#\n"
        "#......................#\n"
        "#......................#\n"
        "#.S...............%#####\n"
        "####..............######\n"
        "####..............######\n"
        "####..............######\n"
        "#.................######\n"
        "########################\n";

    // Tied off standing at the start, hooked at `hook`.
    void tieOffAt(Level& level, Player& player, sf::Vector2f hook)
    {
        player.setCanTie(true);
        player.setRopeRange(130.f);
        player.respawn(level.getStartPos());
        run(level, player, {}, 0.2f);
        hookAt(level, player, hook);
        ASSERT_NE(player.getRope(), nullptr);
        PlayerInput tie;
        tie.ropeHeld = true;
        tie.tiePressed = true;
        run(level, player, tie, DT);
    }
}

TEST(Mechanics, TiedOffTheRopeStaysStrungAndUntiedItComesBack) {
    auto level = Level::loadFromString(ACROSS);
    Player player(level);
    tieOffAt(*level, player, {144.f, 44.f});
    EXPECT_TRUE(player.isTied());
    EXPECT_EQ(player.getRope(), nullptr);
    ASSERT_EQ(player.tiedLine().size(), 2u);
    EXPECT_NEAR(player.tiedLine().back().y, level->getStartPos().y, 1.f); // the knot, at your feet
    run(*level, player, {}, 1.f);                       // let go of the button: it stays
    EXPECT_TRUE(player.isTied());
    EXPECT_FALSE(player.canShootRope());                 // it's not in your hand
    PlayerInput untie;
    untie.tiePressed = true;
    run(*level, player, untie, DT);
    EXPECT_FALSE(player.isTied());
    run(*level, player, {}, 2.f);
    EXPECT_TRUE(player.canShootRope());
}

TEST(Mechanics, TheRopeIsntTiedOffWithoutTheKnotOrInMidAir) {
    auto level = Level::loadFromString(ACROSS);
    Player player(level);
    player.setRopeRange(130.f);
    run(*level, player, {}, 0.2f);
    hookAt(*level, player, {144.f, 44.f});
    PlayerInput tie;
    tie.ropeHeld = true;
    tie.tiePressed = true;
    run(*level, player, tie, DT);
    EXPECT_FALSE(player.isTied()); // no knot
    player.setCanTie(true);
    PlayerInput off;
    off.ropeHeld = true;
    off.moveX = 1.f;
    off.moveY = -1.f;
    run(*level, player, off, 0.4f); // off the ledge, swinging
    ASSERT_FALSE(player.isGrounded());
    EXPECT_FALSE(player.canTie());
    run(*level, player, tie, DT);
    EXPECT_FALSE(player.isTied());
}

TEST(Mechanics, OffTheLedgeYouGrabTheLineAndGoAlongItHandOverHand) {
    auto level = Level::loadFromString(ACROSS);
    Player player(level);
    tieOffAt(*level, player, {144.f, 44.f});
    run(*level, player, moving(1.f), 0.5f); // off the edge, and onto it
    ASSERT_TRUE(player.isHanging());
    EXPECT_GT(player.getPosition().y, 20.f); // caught, not fallen
    const sf::Vector2f at = player.getPosition();
    run(*level, player, moving(1.f), 1.f);
    EXPECT_NEAR(player.getPosition().x - at.x, physics().lineHandSpeed, 8.f);
    const float x = player.getPosition().x;
    run(*level, player, {}, 0.5f); // holding on
    EXPECT_TRUE(player.isHanging());
    EXPECT_NEAR(player.getPosition().x, x, 2.f);
    PlayerInput jump;
    jump.jumpPressed = jump.jumpHeld = true;
    run(*level, player, jump, DT * 2.f);
    EXPECT_FALSE(player.isHanging());
    EXPECT_GT(player.getVelocity().y, 0.f);
}

TEST(Mechanics, WhereTheLineIsSteepYouSlideDownIt) {
    auto level = Level::loadFromString(
        "########################\n"
        "#......................#\n"
        "#..S...................#\n"
        "####...................#\n"
        "####...................#\n"
        "####...................#\n"
        "####...................#\n"
        "####......%%%..........#\n"
        "####......###..........#\n"
        "########################\n");
    Player player(level);
    tieOffAt(*level, player, {84.f, 22.f});
    ASSERT_TRUE(player.isTied());
    run(*level, player, moving(1.f), 0.4f); // off the top...
    for (int ii = 0; ii < static_cast<int>(1.f / DT) && !player.isHanging(); ++ii) run(*level, player, {}, DT); // ...onto it
    ASSERT_TRUE(player.isHanging());
    float fastest = 0.f, furthest = 0.f;
    for (int ii = 0; ii < static_cast<int>(0.5f / DT); ++ii) {
        run(*level, player, {}, DT);
        fastest = std::max(fastest, magnitude(player.getVelocity()));
        furthest = std::max(furthest, player.getPosition().x);
    }
    EXPECT_GT(fastest, physics().lineHandSpeed * 2.f); // sliding, not hand over hand
    EXPECT_GT(furthest, 60.f);                         // down it, towards the hook
    EXPECT_TRUE(player.isHanging());                   // and holding on at the end
}

TEST(Mechanics, ABreathingFireBurnsHalfTheTimeAndFlickersFirst) {
    auto level = Level::loadFromString("##########\n#........#\n#........#\n#.S......#\n##########\n");
    level->addFire({5, 1}, 2.f, 0.f, true); // a flue: burning a second, cold a second
    const Fire& flue = level->fires()[0];
    EXPECT_FLOAT_EQ(level->fireBox(flue).height, 3.f * Level::TILE_SIZE); // up to the roof
    Player player(level);
    EXPECT_TRUE(level->isBurning(flue));
    run(*level, player, {}, 1.2f);
    EXPECT_FALSE(level->isBurning(flue));
    EXPECT_FALSE(level->isAboutToBurn(flue));
    EXPECT_FALSE(level->burnsAlong({44.f, 20.f}, {44.f, 30.f}));
    run(*level, player, {}, 0.6f);
    EXPECT_TRUE(level->isAboutToBurn(flue)); // a moment before it lights
    run(*level, player, {}, 0.3f);
    EXPECT_TRUE(level->isBurning(flue));
    EXPECT_TRUE(level->burnsAlong({44.f, 20.f}, {44.f, 30.f}));
}

TEST(Mechanics, InTheCuttingTheStoneIsAStepUpToTheKnot) {
    // The way the room's made to be done: under the ledge, wait for the flue to go cold, hook the
    // stone, reel it in to your feet, step up onto it, and up onto the ledge.
    std::shared_ptr<Level> level;
    for (auto& room : loadWorld(config::assetPath("world.ldtk"))) {
        if (room.name == "cutting") level = room.level;
    }
    ASSERT_TRUE(level);
    Player player(level);
    player.respawn({72.f, 32.f});
    const Fire& flue = level->fires()[0];
    while (!level->isBurning(flue)) run(*level, player, {}, DT); // too short a while: let it burn
    while (level->isBurning(flue)) run(*level, player, {}, DT);  // and go out
    hookAt(*level, player, level->stones()[0].position + sf::Vector2f(1.f, 8.f));
    ASSERT_NE(player.getRope(), nullptr);
    PlayerInput reel;
    reel.ropeHeld = true;
    reel.moveY = 1.f;
    run(*level, player, reel, 1.6f);
    EXPECT_FLOAT_EQ(level->stones()[0].position.x, player.getPosition().x + Player::WIDTH); // at your feet
    run(*level, player, {}, 0.5f);                        // let go of it
    PlayerInput onto;
    onto.moveX = 1.f;
    onto.jumpPressed = onto.jumpHeld = true;
    run(*level, player, onto, 0.25f);
    run(*level, player, {}, 0.5f);
    EXPECT_FLOAT_EQ(player.getPosition().y, 48.f); // on the stone
    PlayerInput up;
    up.moveX = -1.f;
    up.jumpPressed = up.jumpHeld = true;
    run(*level, player, up, 0.5f);
    run(*level, player, {}, 0.5f);
    EXPECT_FLOAT_EQ(player.getPosition().y, 72.f); // on the ledge
    // Without the stone, a jump from the floor doesn't make it.
    player.respawn({76.f, 32.f});
    level->reset();
    PlayerInput jump;
    jump.moveX = -1.f;
    jump.jumpPressed = jump.jumpHeld = true;
    run(*level, player, jump, 0.6f);
    run(*level, player, {}, 0.5f);
    EXPECT_FLOAT_EQ(player.getPosition().y, 32.f);
}
