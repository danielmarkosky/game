// Finds out whether a room can be got through, by trying moves with the real player physics: a
// search over the spots the player can stand on, trying runs, jumps and rope swings from each.
// It doesn't play like a person, but if it gets through, a person can too; if it can't, the room
// is probably too hard, or impossible.
//
// usage: levelbot <room> [--start COL ROW] [--goal L|R|T|B | --goal pickup | --goal gem N]
//                        [--abilities a,b,...] [--explore]
//        levelbot --measure
//   <room>       a room's identifier in assets/world.ldtk, or a text room file
//   --start      where to start: the tile to stand in, column and row from the top-left
//                (default: the room's Start)
//   --goal       which way out counts (default: any), or touching a pickup, or gem N, or "tug": the
//                rope tugging a lever or a lamp, or "item": touching a quest item
//   --abilities  the ones the player has (default: all). "none" for none.
//   --explore    don't stop at the goal: list every spot that can be stood on
//   --open       with the room's gates open, as if its lever had been pulled
//   --measure    how far the moves reach with the physics in assets/physics.txt, for designing rooms
//
// Prints the route it found and exits with 0, or prints a map of where it got to and exits with 1.
#include "src/Config.h"
#include "src/Level.h"
#include "src/Physics.h"
#include "src/Player.h"
#include "src/World.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    constexpr float DT = config::FIXED_DT;

    enum class Out { Landed, Died, Reached, Timeout, Missed };
    struct Result
    {
        Out out;
        sf::Vector2f pos;
    };

    struct Goal
    {
        std::optional<Level::Side> side; // none and no pickup or gem: any way out
        bool pickup = false;
        int gem = -1;
        bool tug = false; // the rope tugging a lever or a lamp
        bool item = false; // touching a quest item
    };

    // A run, then maybe a jump, held for a while, steering in the air, maybe jumping again.
    struct Jump
    {
        float run, runTime, hold, steer;
        bool drop;        // hold down first, to drop through a platform
        float airJumpAt;  // < 0: no second jump
    };

    // Maybe a jump first, then the rope thrown at an angle, swung on for a while once it catches,
    // let go of.
    struct Swing
    {
        bool jumpFirst;
        float angle, push;
        bool reel;
        float hold;
        bool jumpOff;     // let go by jumping: only different when the physics give that a kick
        float steer;
        bool airJump;
        float again = -1.f; // with the throwing hand: thrown again at this angle as soon as it's back, < 0 not
        float holdAgain = 0.f;
        float wait = 0.f;   // standing first: in a room with moving rings, for one to come round
        int ring = -1;      // thrown straight at this ring, wherever it is by then, not at `angle`
        std::optional<sf::Vector2f> at = std::nullopt; // or straight at this point: a lever or a lamp
    };

    // Up against a wall (with the chalk bag): run and jump at it holding towards it until it's
    // grabbed, climb for a while (over the top, if it's long enough), then leap off or let go.
    struct Climb
    {
        float dir;     // the wall's side
        float time;    // climbing up
        bool leap;
        float steer;
        int leaps = 0; // first, leaping back and forth to the wall across this many times
    };

    constexpr float THROW_WAIT = 1.5f; // the longest a throw is waited on to catch

    class Bot
    {
    public:
        Bot(std::shared_ptr<Level> lvl, Goal g) : level(std::move(lvl)), goal(g), player(level) {}

        std::shared_ptr<Level> level;
        Goal goal;
        Player player;
        long long steps = 0;

        Result jump(sf::Vector2f spot, const Jump& j)
        {
            start(spot);
            PlayerInput in;
            in.moveX = j.run;
            if (j.drop) in.moveY = -1.f;
            bool airborne = false;
            for (int i = 0; i < int(j.runTime / DT); ++i) {
                if (auto o = step(in)) return {*o, player.getPosition()};
                airborne |= !player.isGrounded();
            }
            if (j.hold > 0.f) {
                PlayerInput jump;
                jump.moveX = j.steer;
                jump.jumpPressed = jump.jumpHeld = true;
                for (int i = 0; i < int(j.hold / DT); ++i) {
                    if (auto o = step(jump)) return {*o, player.getPosition()};
                    jump.jumpPressed = false;
                    airborne |= !player.isGrounded();
                }
            }
            if (j.airJumpAt >= 0.f) {
                PlayerInput fall;
                fall.moveX = j.steer;
                for (int i = 0; i < int(j.airJumpAt / DT); ++i) {
                    if (auto o = step(fall)) return {*o, player.getPosition()};
                }
                if (player.isGrounded()) return {Out::Missed, {}};
                PlayerInput again = fall;
                again.jumpPressed = again.jumpHeld = true;
                for (int i = 0; i < int(0.3f / DT); ++i) {
                    if (auto o = step(again)) return {*o, player.getPosition()};
                    again.jumpPressed = false;
                }
                airborne = true;
            }
            return settle(j.steer, airborne);
        }

        Result swing(sf::Vector2f spot, const Swing& s)
        {
            start(spot);
            for (int i = 0; i < int(s.wait / DT); ++i) {
                if (auto o = step({})) return {*o, player.getPosition()};
            }
            PlayerInput in;
            in.moveX = s.push;
            if (s.jumpFirst) {
                in.jumpPressed = in.jumpHeld = true;
                for (int i = 0; i < int(0.15f / DT); ++i) {
                    if (auto o = step(in)) return {*o, player.getPosition()};
                    in.jumpPressed = false;
                }
            }
            PlayerInput rope;
            rope.ropePressed = rope.ropeHeld = true;
            rope.jumpHeld = s.jumpFirst;
            rope.aim = player.centre() + sf::Vector2f(std::cos(s.angle), std::sin(s.angle)) * 200.f;
            if (s.ring >= 0) rope.aim = level->ringPosition(static_cast<size_t>(s.ring));
            if (s.at) rope.aim = *s.at;
            if (goal.tug && player.canShootRope()) {
                // Thrown at a lever or a lamp, all there is to it is the tug.
                if (const auto hit = player.aimHit(rope.aim); hit && (hit->kind == RopeHit::Kind::Lever || hit->kind == RopeHit::Kind::Lamp)) {
                    for (int i = 0; i < int(THROW_WAIT / DT); ++i) {
                        if (auto o = step(rope)) return {*o, player.getPosition()};
                        rope.ropePressed = false;
                    }
                }
            }
            if (!player.canShootRope() || !player.ropeTarget(rope.aim)) return {Out::Missed, {}};
            // Thrown (swung round first, early on), and flying there. Standing still meanwhile, or
            // in the air steering on.
            rope.moveX = s.jumpFirst ? s.push : 0.f;
            for (int i = 0; i < int(THROW_WAIT / DT) && !player.getRope(); ++i) {
                if (auto o = step(rope)) return {*o, player.getPosition()};
                rope.ropePressed = false;
            }
            if (!player.getRope()) return {Out::Missed, {}};
            rope.ropePressed = false;
            rope.moveX = s.push;
            rope.moveY = s.reel ? 1.f : 0.f;
            for (int i = 0; i < int(s.hold / DT); ++i) {
                if (auto o = step(rope)) return {*o, player.getPosition()};
            }
            PlayerInput let;
            let.moveX = s.steer;
            if (s.jumpOff) let.jumpPressed = let.jumpHeld = true;
            for (int i = 0; i < int(0.3f / DT); ++i) {
                if (auto o = step(let)) return {*o, player.getPosition()};
                let.jumpPressed = false;
                if (player.isGrounded()) break;
            }
            if (s.airJump && !player.isGrounded()) {
                let.jumpPressed = let.jumpHeld = true;
                for (int i = 0; i < int(0.3f / DT); ++i) {
                    if (auto o = step(let)) return {*o, player.getPosition()};
                    let.jumpPressed = false;
                }
            }
            if (s.again >= 0.f && !player.isGrounded()) {
                // Pressed again straight away: thrown as soon as it's back in the hand, mid-air.
                PlayerInput next;
                next.moveX = s.steer;
                next.ropePressed = next.ropeHeld = true;
                const sf::Vector2f direction(std::cos(s.again), std::sin(s.again));
                for (int i = 0; i < int(THROW_WAIT / DT) && !player.getRope(); ++i) {
                    next.aim = player.centre() + direction * 200.f;
                    if (auto o = step(next)) return {*o, player.getPosition()};
                    next.ropePressed = false;
                    if (player.isGrounded()) break;
                }
                if (!player.getRope()) return {Out::Missed, {}};
                next.moveX = s.push;
                for (int i = 0; i < int(s.holdAgain / DT); ++i) {
                    if (auto o = step(next)) return {*o, player.getPosition()};
                }
            }
            return settle(s.steer, true);
        }

        Result climb(sf::Vector2f spot, const Climb& c)
        {
            start(spot);
            PlayerInput at;
            at.moveX = c.dir;
            at.moveY = 1.f; // grabs it from the ground too
            at.jumpPressed = at.jumpHeld = true;
            for (int i = 0; i < int(1.2f / DT) && !player.isClinging(); ++i) {
                if (auto o = step(at)) return {*o, player.getPosition()};
                at.jumpPressed = false;
            }
            if (!player.isClinging()) return {Out::Missed, {}};
            // Back and forth up a chimney: leap off, steer across, grab the other wall.
            float side = c.dir;
            for (int leap = 0; leap < c.leaps; ++leap) {
                PlayerInput off;
                off.moveX = -side;
                off.moveY = 1.f;
                off.jumpPressed = off.jumpHeld = true;
                bool held = false;
                for (int i = 0; i < int(0.8f / DT); ++i) {
                    if (auto o = step(off)) return {*o, player.getPosition()};
                    off.jumpPressed = false;
                    if (i > 2 && player.isClinging()) {
                        held = true;
                        break;
                    }
                }
                if (!held) return settle(-side, true);
                side = -side;
            }
            PlayerInput up;
            up.moveX = side;
            up.moveY = 1.f;
            for (int i = 0; i < int(c.time / DT) && player.isClinging(); ++i) {
                if (auto o = step(up)) return {*o, player.getPosition()};
            }
            if (!player.isClinging()) return settle(side, true); // over the top
            PlayerInput off;
            off.moveX = c.steer;
            if (c.leap) off.jumpPressed = off.jumpHeld = true;
            for (int i = 0; i < int(0.3f / DT); ++i) {
                if (auto o = step(off)) return {*o, player.getPosition()};
                off.jumpPressed = false;
            }
            return settle(c.steer, true);
        }

        // Every try from a spot starts with the room as it was: icicles up, blocks whole, rings at the start.
        void start(sf::Vector2f spot)
        {
            level->reset();
            player.respawn(spot);
        }

    private:
        std::optional<Out> step(const PlayerInput& in)
        {
            player.handleInput(in);
            level->update(DT, player.hitbox());
            player.update(DT);
            bool tugged = false;
            for (const PlayerEvent& e : player.takeEvents()) {
                tugged |= e.type == PlayerEvent::Type::LeverTugged || e.type == PlayerEvent::Type::LampTugged;
            }
            ++steps;
            if (goal.tug && tugged) return Out::Reached;
            if (goal.item && level->questItemAt(player.hitbox())) return Out::Reached;
            if (goal.pickup && level->pickupAt(player.getPosition(), Player::HEIGHT, Player::WIDTH)) return Out::Reached;
            if (goal.gem >= 0 && level->gemAt(player.getPosition(), Player::HEIGHT, Player::WIDTH) == size_t(goal.gem)) return Out::Reached;
            if (player.isTouchingDanger() || player.getPosition().y < level->bounds().top - 64.f) return Out::Died;
            if (auto side = level->exitThrough(player.centre())) {
                const bool wanted = !goal.pickup && !goal.tug && !goal.item && goal.gem < 0 && (!goal.side || *side == *goal.side);
                return wanted ? Out::Reached : Out::Died;
            }
            return std::nullopt;
        }

        // Steers until landing, then waits to come to rest.
        Result settle(float steer, bool airborne)
        {
            PlayerInput in;
            in.moveX = steer;
            for (int i = 0; i < int(4.f / DT); ++i) {
                const bool grounded = player.isGrounded() && !player.getRope();
                if (grounded && (airborne || i > 0)) break;
                airborne |= !grounded;
                if (auto o = step(in)) return {*o, player.getPosition()};
                if (i == int(4.f / DT) - 1) return {Out::Timeout, {}};
            }
            for (int i = 0; i < int(0.3f / DT); ++i) {
                if (auto o = step({})) return {*o, player.getPosition()};
            }
            if (!player.isGrounded()) return {Out::Timeout, {}};
            return {Out::Landed, player.getPosition()};
        }

    };

    std::string describe(const Jump& j)
    {
        char b[160];
        std::snprintf(b, sizeof b, "%s run=%+.0f for %.2fs, jump %.2fs steering %+.0f%s", j.drop ? "drop" : "jump", j.run, j.runTime, j.hold, j.steer,
                      j.airJumpAt >= 0.f ? (", jump again after " + std::to_string(j.airJumpAt).substr(0, 4) + "s").c_str() : "");
        return b;
    }

    std::string describe(const Climb& c)
    {
        char b[160];
        std::snprintf(b, sizeof b, "climb the wall to the %s, leaping across %d times, climb for %.2fs, %s steering %+.0f",
                      c.dir < 0.f ? "left" : "right", c.leaps, c.time, c.leap ? "leap off" : "let go", c.steer);
        return b;
    }

    std::string describe(const Swing& s)
    {
        char b[160];
        if (s.at) return "rope straight at the lever or lamp at " + std::to_string(int(s.at->x)) + "," + std::to_string(int(s.at->y));
        if (s.ring >= 0) {
            std::snprintf(b, sizeof b, "after %.1fs, rope at ring %d, push %+.0f%s for %.2fs once it catches, let go steering %+.0f", s.wait, s.ring,
                          s.push, s.reel ? " reeling in" : "", s.hold, s.steer);
            return b;
        }
        std::snprintf(b, sizeof b, "rope%s at %.0f deg, push %+.0f%s for %.2fs once it catches, %s steering %+.0f%s", s.jumpFirst ? " mid-jump" : "",
                      s.angle * 57.2958f, s.push, s.reel ? " reeling in" : "", s.hold, s.jumpOff ? "jump off" : "let go", s.steer,
                      s.airJump ? ", jump again" : "");
        std::string text = b;
        if (s.again >= 0.f) {
            std::snprintf(b, sizeof b, ", then the rope again in the air at %.0f deg for %.2fs", s.again * 57.2958f, s.holdAgain);
            text += b;
        }
        return text;
    }
}

namespace
{
    const std::set<std::string> ALL_ABILITIES = {"rope", "long_rope", "wall_climb", "air_throw", "double_jump"};

    void giveAbilities(Player& player, const std::set<std::string>& abilities)
    {
        player.setHasRope(abilities.count("rope") > 0);
        player.setRopeRange(abilities.count("long_rope") ? physics().longRopeRange : physics().ropeRange);
        player.setCanClimb(abilities.count("wall_climb") > 0);
        player.setCanThrowInAir(abilities.count("air_throw") > 0);
        player.setCanDoubleJump(abilities.count("double_jump") > 0);
    }

    // An empty room `width` tiles wide with a floor, the start near its left, and `ceiling` empty
    // rows above the floor under a ceiling of hook blocks (none: open to the top of 40 rows).
    // `wall`: a wall that high at column 20.
    std::shared_ptr<Level> testRoom(int width, int ceiling = 0, int wall = 0)
    {
        const int rows = ceiling > 0 ? ceiling : 40;
        std::string text = std::string(width, ceiling > 0 ? '%' : '.') + "\n";
        for (int r = rows; r >= 1; --r) {
            std::string row(width, '.');
            if (wall >= r) row[20] = '#';
            if (r == 1) row[10] = 'S';
            text += row + "\n";
        }
        text += std::string(width, '#') + "\n";
        return Level::loadFromString(text);
    }

    // Steps `player` with `in` (presses only at first) for up to `seconds`, calling `each` after
    // every step until it says stop.
    template <typename F>
    void run(Player& player, PlayerInput in, float seconds, F each)
    {
        for (int i = 0; i < int(seconds / DT); ++i) {
            player.handleInput(in);
            player.update(DT);
            in.jumpPressed = in.ropePressed = false;
            if (!each()) return;
        }
    }

    // A jump on an open floor: standing or at full speed, maybe jumping again at the top. How high
    // the feet get and how far it lands, in tiles.
    std::pair<float, float> measureJump(bool running, bool again)
    {
        auto level = testRoom(200);
        Player player(level);
        giveAbilities(player, ALL_ABILITIES);
        const float floor = player.getPosition().y;
        PlayerInput in;
        in.moveX = running ? 1.f : 0.f;
        run(player, in, 1.f, [] { return true; });
        const float from = player.getPosition().x;
        in.jumpPressed = in.jumpHeld = true;
        float top = floor;
        bool secondDone = !again;
        run(player, in, 3.f, [&] {
            top = std::max(top, player.getPosition().y);
            if (!secondDone && player.getVelocity().y <= 0.f) {
                PlayerInput twice = in;
                twice.jumpPressed = true;
                player.handleInput(twice);
                secondDone = true;
            }
            return !player.isGrounded() || player.getPosition().y > floor;
        });
        return {(top - floor) / Level::TILE_SIZE, (player.getPosition().x - from) / Level::TILE_SIZE};
    }

    // Climbing a high wall from the ground until the grip runs out: how high the feet get, in tiles.
    float measureClimb()
    {
        auto level = testRoom(40, 0, 38);
        Player player(level);
        giveAbilities(player, ALL_ABILITIES);
        const float floor = player.getPosition().y;
        PlayerInput in;
        in.moveX = 1.f;
        in.moveY = 1.f;
        float top = floor;
        run(player, in, 8.f, [&] {
            top = std::max(top, player.getPosition().y);
            return player.staminaLeft() > 0.f || player.isClinging();
        });
        return (top - floor) / Level::TILE_SIZE;
    }

    // The furthest one swing carries along a flat floor, landing on it, from under a ceiling of hook
    // blocks `ceiling` tiles above it, in tiles. Tries what the room search tries.
    float measureSwing(int ceiling, bool longRope)
    {
        auto level = testRoom(200, ceiling);
        Bot bot(level, Goal{});
        std::set<std::string> abilities = ALL_ABILITIES;
        if (!longRope) abilities.erase("long_rope");
        abilities.erase("double_jump");
        giveAbilities(bot.player, abilities);
        bot.player.respawn(level->getStartPos());
        run(bot.player, {}, 0.5f, [] { return true; });
        const sf::Vector2f spot = bot.player.getPosition();
        float furthest = 0.f;
        for (bool jumpFirst : {false, true}) {
            for (int deg = 30; deg <= 90; deg += 5) {
                for (bool reel : {false, true}) {
                    for (float hold = 0.2f; hold < 2.5f; hold += 0.05f) {
                        const Result r = bot.swing(spot, {jumpFirst, deg / 57.2958f, 1.f, reel, hold, false, 1.f, false});
                        if (r.out == Out::Landed) furthest = std::max(furthest, r.pos.x - spot.x);
                    }
                }
            }
        }
        return furthest / Level::TILE_SIZE;
    }

    int measure()
    {
        const Physics& p = physics();
        std::printf("With the physics in assets/physics.txt (in tiles of 8 px):\n");
        const auto [standing, standingAcross] = measureJump(false, false);
        const auto [running, across] = measureJump(true, false);
        const auto [twice, twiceAcross] = measureJump(true, true);
        std::printf("  jump: %.1f up standing; at a run %.1f up, %.1f across\n", standing, running, across);
        std::printf("  with the feather, again at the top: %.1f up, %.1f across\n", twice, twiceAcross);
        (void)standingAcross;
        std::printf("  rope: reaches %.1f standing, %.1f in the air; long rope %.1f and %.1f\n", p.ropeRange / 8.f,
                    p.ropeRange * p.airThrowRange / 8.f, p.longRopeRange / 8.f, p.longRopeRange * p.airThrowRange / 8.f);
        std::printf("  climbing: up %.1f from the ground before the grip runs out\n", measureClimb());
        std::printf("  one swing along the floor under a ceiling of hooks (rope / long rope):\n");
        for (int ceiling : {5, 7, 9, 11}) {
            std::printf("    ceiling %2d up: %4.1f / %4.1f across\n", ceiling, measureSwing(ceiling, false), measureSwing(ceiling, true));
        }
        std::fflush(stdout);
        std::_Exit(0);
    }
}

int main(int argc, char** argv)
{
    // Moving as in the game.
    for (const std::string& problem : loadPhysics(config::assetPath("physics.txt"))) std::cerr << "physics.txt: " << problem << "\n";
    if (argc >= 2 && std::string(argv[1]) == "--measure") return measure();
    if (argc < 2) {
        std::cerr << "usage: levelbot <room> [--start COL ROW] [--goal L|R|T|B|pickup|gem N] [--abilities a,b] [--explore]\n";
        return 2;
    }
    const std::string roomArg = argv[1];
    std::optional<sf::Vector2i> startTile;
    Goal goal;
    std::set<std::string> abilities = ALL_ABILITIES;
    bool explore = false, openGates = false;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--start" && i + 2 < argc) {
            startTile = sf::Vector2i(std::atoi(argv[i + 1]), std::atoi(argv[i + 2]));
            i += 2;
        } else if (a == "--goal" && i + 1 < argc) {
            const std::string g = argv[++i];
            if (g == "pickup") goal.pickup = true;
            else if (g == "tug") goal.tug = true;
            else if (g == "item") goal.item = true;
            else if (g == "gem" && i + 1 < argc) goal.gem = std::atoi(argv[++i]);
            else goal.side = g == "L" ? Level::Side::Left : g == "R" ? Level::Side::Right : g == "T" ? Level::Side::Top : Level::Side::Bottom;
        } else if (a == "--abilities" && i + 1 < argc) {
            abilities.clear();
            std::stringstream list(argv[++i]);
            for (std::string name; std::getline(list, name, ',');) {
                if (name != "none") abilities.insert(name);
            }
        } else if (a == "--explore") {
            explore = true;
        } else if (a == "--open") {
            openGates = true;
        }
    }

    std::shared_ptr<Level> level;
    if (roomArg.size() > 4 && roomArg.substr(roomArg.size() - 4) == ".txt") {
        level = loadSingleLevel(roomArg).level;
    } else {
        for (auto& room : loadWorld(config::assetPath("world.ldtk"))) {
            if (room.name == roomArg) level = room.level;
        }
    }
    if (!level) {
        std::cerr << "No room called " << roomArg << "\n";
        return 2;
    }

    level->setGatesOpen(openGates);
    Bot bot(level, goal);
    Player& player = bot.player;
    giveAbilities(player, abilities);
    const bool doubleJump = abilities.count("double_jump") > 0;
    const bool rope = abilities.count("rope") > 0;
    const bool airThrow = abilities.count("air_throw") > 0;
    const bool climbs = abilities.count("wall_climb") > 0;
    const bool ropeJumpKick = physics().ropeJumpSpeed > 0.f;

    const sf::FloatRect b = level->bounds();
    const int rows = static_cast<int>(b.height / Level::TILE_SIZE);
    sf::Vector2f start = level->getStartPos();
    if (startTile) start = {startTile->x * Level::TILE_SIZE, (rows - 1 - startTile->y) * Level::TILE_SIZE + b.top};

    // The moves to try from every spot.
    std::vector<Jump> jumps;
    for (float run : {-1.f, 0.f, 1.f}) {
        for (float runTime : {0.f, 0.08f, 0.18f, 0.35f, 0.6f}) {
            for (float hold : {0.f, 0.06f, 0.14f, 0.5f}) {
                for (float steer : {-1.f, 0.f, 1.f}) {
                    if (hold == 0.f && (runTime == 0.f || steer != run)) continue;
                    if (run == 0.f && runTime > 0.f) continue;
                    jumps.push_back({run, runTime, hold, steer, false, -1.f});
                    if (doubleJump && hold == 0.5f) {
                        // Again as it starts falling, and right at the top, where it goes highest.
                        for (float at : {0.f, 0.15f}) jumps.push_back({run, runTime, hold, steer, false, at});
                        jumps.push_back({run, runTime, 0.26f, steer, false, 0.f});
                    }
                }
            }
        }
    }
    for (float steer : {-1.f, 0.f, 1.f}) jumps.push_back({0.f, 0.1f, 0.f, steer, true, -1.f});
    std::vector<Swing> swings;
    for (bool jumpFirst : {false, true}) {
        if (!rope || (jumpFirst && !airThrow)) continue; // thrown standing only, until the throwing hand
        for (int deg = 25; deg <= 155; deg += 10) {
            for (float push : {-1.f, 1.f}) {
                for (bool reel : {false, true}) {
                    for (float hold : {0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.8f, 2.2f}) {
                        for (bool jumpOff : {false, true}) {
                            if (jumpOff && !ropeJumpKick) continue; // jumping only lets go, like letting go
                            for (float steer : {push, 0.f, -push}) {
                                swings.push_back({jumpFirst, deg / 57.2958f, push, reel, hold, jumpOff, steer, false});
                                if (doubleJump && steer == push) swings.push_back({jumpFirst, deg / 57.2958f, push, reel, hold, jumpOff, steer, true});
                                // With the throwing hand: let go and throw again ahead, mid-air.
                                if (airThrow && steer == push && !jumpFirst && !reel) {
                                    for (int again : {40, 60, 80}) {
                                        const float angle = (push > 0.f ? again : 180 - again) / 57.2958f;
                                        for (float holdAgain : {0.5f, 1.0f, 1.5f}) {
                                            Swing chained{jumpFirst, deg / 57.2958f, push, reel, hold, jumpOff, steer, false};
                                            chained.again = angle;
                                            chained.holdAgain = holdAgain;
                                            swings.push_back(chained);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // When it's a tug that's wanted: straight at each lever and lamp.
    if (goal.tug) {
        std::vector<sf::Vector2f> targets;
        for (const sf::Vector2f& lever : level->levers()) targets.push_back(lever + sf::Vector2f(4.f, 4.f));
        for (const Lamp& lamp : level->lamps()) targets.push_back(lamp.position + sf::Vector2f(4.f, 4.f));
        for (const sf::Vector2f& target : targets) {
            Swing tug{false, 0.f, 1.f, false, 0.2f, false, 0.f, false};
            tug.at = target;
            swings.push_back(tug);
        }
    }
    // Straight at each ring, at moments as it goes round.
    for (int ring = 0; ring < static_cast<int>(level->rings().size()); ++ring) {
        for (float wait : {0.f, 0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f}) {
            for (float push : {-1.f, 1.f}) {
                for (bool reel : {false, true}) {
                    for (float hold : {0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.f, 2.5f}) {
                        for (float steer : {push, 0.f}) {
                            Swing atRing{false, 0.f, push, reel, hold, false, steer, false};
                            atRing.wait = wait;
                            atRing.ring = ring;
                            swings.push_back(atRing);
                        }
                    }
                }
            }
        }
    }
    // With rings going round, the same swing a moment later is another swing.
    if (!level->rings().empty()) {
        const size_t count = swings.size();
        for (float wait : {0.5f, 1.0f, 1.5f, 2.0f}) {
            for (size_t i = 0; i < count; ++i) {
                if (swings[i].jumpFirst || swings[i].again >= 0.f) continue;
                Swing later = swings[i];
                later.wait = wait;
                swings.push_back(later);
            }
        }
    }
    std::vector<Climb> climbing;
    for (float dir : {-1.f, 1.f}) {
        if (!climbs) continue;
        for (float time : {0.f, 0.4f, 0.8f, 1.2f, 1.7f}) {
            for (float steer : {-dir, 0.f, dir}) climbing.push_back({dir, time, true, steer});
            for (float steer : {-dir, 0.f}) climbing.push_back({dir, time, false, steer});
        }
        climbing.push_back({dir, 5.f, false, 0.f}); // as high as it goes: over the top
        // Up a chimney, wall to wall, then as high as it goes.
        for (int leaps : {2, 4, 6, 8}) climbing.push_back({dir, 5.f, false, 0.f, leaps});
    }

    // Best-first, furthest from the start first: where the goal is isn't known.
    struct Node
    {
        sf::Vector2f pos;
        int parent;
        std::string how;
    };
    std::vector<Node> nodes;
    std::map<std::pair<int, int>, int> seen;
    auto key = [](sf::Vector2f p) { return std::make_pair(int(std::floor(p.x / 6.f)), int(p.y)); };
    auto priority = [&](int i) { return std::abs(nodes[i].pos.x - start.x) + std::abs(nodes[i].pos.y - start.y); };
    std::priority_queue<std::pair<float, int>> open;

    player.respawn(start);
    PlayerInput none;
    for (int i = 0; i < int(1.f / DT); ++i) {
        player.handleInput(none);
        player.update(DT);
    }
    nodes.push_back({player.getPosition(), -1, "start"});
    seen[key(nodes[0].pos)] = 0;
    open.push({priority(0), 0});

    int found = -1;
    std::string foundHow;
    while (!open.empty() && found < 0) {
        const int i = open.top().second;
        open.pop();
        const sf::Vector2f spot = nodes[i].pos;
        auto handle = [&](Result r, const std::string& how) {
            if (r.out == Out::Reached && !explore) {
                found = i;
                foundHow = how;
                return true;
            }
            if (r.out != Out::Landed || seen.count(key(r.pos))) return false;
            seen[key(r.pos)] = static_cast<int>(nodes.size());
            nodes.push_back({r.pos, i, how});
            open.push({priority(static_cast<int>(nodes.size()) - 1), static_cast<int>(nodes.size()) - 1});
            return false;
        };
        bool done = false;
        for (const Jump& j : jumps) {
            if ((done = handle(bot.jump(spot, j), describe(j)))) break;
        }
        if (done) break;
        // No wall to grab on one side from here: nothing more to try that way.
        std::set<float> noWall;
        for (const Climb& c : climbing) {
            if (noWall.count(c.dir)) continue;
            const Result r = bot.climb(spot, c);
            if (r.out == Out::Missed) {
                noWall.insert(c.dir);
                continue;
            }
            if ((done = handle(r, describe(c)))) break;
        }
        if (done) break;
        // Once the rope misses at an angle from here, it'll miss whatever else is tried with it.
        std::set<std::tuple<bool, int, int>> misses;
        for (const Swing& s : swings) {
            if (s.at) {
                if ((done = handle(bot.swing(spot, s), describe(s)))) break;
                continue;
            }
            const auto aim = std::make_tuple(s.jumpFirst, s.ring >= 0 ? 1000 + s.ring : int(std::round(s.angle * 57.2958f)), int(s.wait * 10.f));
            if (misses.count(aim)) continue;
            const Result r = bot.swing(spot, s);
            if (r.out == Out::Missed) {
                misses.insert(aim);
                continue;
            }
            if ((done = handle(r, describe(s)))) break;
        }
        if (done) break;
    }

    auto tileOf = [&](sf::Vector2f p) { return sf::Vector2i(int((p.x + Player::WIDTH * 0.5f) / Level::TILE_SIZE), rows - 1 - int((p.y - b.top) / Level::TILE_SIZE)); };
    if (explore) {
        for (const Node& n : nodes) std::printf("spot %d %d\n", tileOf(n.pos).x, tileOf(n.pos).y);
        std::fflush(stdout);
        std::_Exit(0);
    }
    std::printf("%s: %s (%zu spots, %lld steps)\n", roomArg.c_str(), found >= 0 ? "REACHED" : "NOT REACHED", nodes.size(), bot.steps);
    if (found >= 0) {
        std::vector<int> chain;
        for (int i = found; i >= 0; i = nodes[i].parent) chain.push_back(i);
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const Node& n = nodes[*it];
            std::printf("  from column %d row %d: %s\n", tileOf(n.pos).x, tileOf(n.pos).y, n.how.c_str());
        }
        std::printf("  then %s\n", foundHow.c_str());
    } else {
        // Where it got to, marked on the room.
        std::vector<std::string> map(rows, std::string(static_cast<size_t>(b.width / Level::TILE_SIZE), ' '));
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < int(map[0].size()); ++x) {
                const int ty = rows - 1 - y;
                map[y][x] = level->isSolid(x, ty) ? '#' : level->isDangerous(x, ty) ? '^' : level->isPlatform(x, ty) ? '-' : '.';
            }
        }
        for (const Node& n : nodes) {
            const sf::Vector2i t = tileOf(n.pos);
            if (t.y >= 0 && t.y < rows && t.x >= 0 && t.x < int(map[0].size())) map[t.y][t.x] = '@';
        }
        for (const auto& row : map) std::printf("%s\n", row.c_str());
    }
    std::fflush(stdout);
    std::_Exit(found >= 0 ? 0 : 1); // skip tearing down SFML, not needed here
}
