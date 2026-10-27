// Finds out whether a room can be got through, by trying moves with the real player physics: a
// search over the spots the player can stand on, trying runs, jumps and rope swings from each.
// It doesn't play like a person, but if it gets through, a person can too; if it can't, the room
// is probably too hard, or impossible.
//
// usage: levelbot <room> [--start COL ROW] [--goal L|R|T|B | --goal pickup | --goal gem N]
//                        [--abilities a,b,...] [--explore] [--riding 1|-1]
//        levelbot --measure
//   <room>       a room's identifier in assets/world.ldtk, or a text room file
//   --start      where to start: the tile to stand in, column and row from the top-left
//                (default: the room's Start)
//   --goal       which way out counts (default: any; "L:21" only the left door starting 21 up), or
//                touching a pickup, or gem N, or "tug": the
//                rope tugging a lever or a lamp, or "cart": getting into a mine cart, or "stop":
//                riding one to the end of the line, "plate": a stone dragged onto a weight
//                plate, or "at COL ROW": standing in that tile
//   --abilities  the ones the player has (default: all but the knot). "none" for none. "diver" is
//                wearing the diving suit: breath lasting longer; "tie_off" the knot
//   --explore    don't stop at the goal: list every spot that can be stood on
//   --open       with the room's gates open, as if its lever had been pulled
//   --riding     starting in a mine cart rolling that way: all there is to try is when to jump
//   --measure    how far the moves reach with the physics in assets/physics.txt, for designing rooms
//
// In a room with no gravity (a `space` room) the spots are where you come to rest holding on to
// rock, not where you stand, and the moves are the rope thrown every way (reeled in on, or held,
// for a while, then let go of) and clambering along the rock.
//
// Prints the route it found and exits with 0, or prints a map of where it got to and exits with 1.
#include "src/Config.h"
#include "src/Level.h"
#include "src/Physics.h"
#include "src/Player.h"
#include "src/World.h"
#include "src/utils.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
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
        float air = 1.f; // breath left at the end, 0 to 1
    };

    struct Goal
    {
        std::optional<Level::Side> side; // none and no pickup or gem: any way out
        int door = -1;                   // and only the door on that side starting there (as make_world.py counts), if not -1
        bool pickup = false;
        int gem = -1;
        bool tug = false;  // the rope tugging a lever or a lamp
        bool cart = false; // getting into a mine cart
        bool stop = false; // riding one to the end of the line
        bool plate = false; // a stone on a weight plate
        std::optional<sf::Vector2i> at; // standing in this tile (column, and row from the top)
        bool any() const { return pickup || gem >= 0 || tug || cart || stop || plate || at; } // something other than a way out
    };

    // A run, then maybe a jump, held for a while, steering in the air, maybe jumping again.
    struct Jump
    {
        float run, runTime, hold, steer;
        bool drop;        // hold down first, to drop through a platform
        float airJumpAt;  // < 0: no second jump
        float wait = 0.f; // standing first: in a room with fires that come and go, for one to go out
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
        float outFirst = 0.f; // once it catches, let out for this long first (lowering yourself on it)
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

    // With no gravity: pushing one way for a while, then drifting to a stop.
    struct Drift
    {
        sf::Vector2f push; // -1, 0 or 1 each way
        float time;
    };

    // With the knot: the rope thrown at an angle, tied off where you stand once it catches, then
    // onto the line (a jump that way, holding up), along it towards the hook or away for a while
    // (holding up too, which climbs it where it's steep), and a jump off it, steering.
    struct Tie
    {
        float angle;
        float onto;    // the way to jump onto it: -1 or 1
        float along;   // then the way to go along it: 1 towards the hook, -1 away
        float time;
        float steer;   // jumping off it
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
            for (int i = 0; i < int(j.wait / DT); ++i) {
                if (auto o = step({})) return {*o, player.getPosition()};
            }
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
            rope.moveY = -1.f;
            for (int i = 0; i < int(s.outFirst / DT); ++i) {
                if (auto o = step(rope)) return {*o, player.getPosition()};
            }
            rope.moveY = s.reel ? 1.f : 0.f;
            if (s.outFirst > 0.f) rope.moveX = 0.f; // lowered down: standing still to reel it in
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

        Result tie(sf::Vector2f spot, const Tie& t)
        {
            start(spot);
            PlayerInput rope;
            rope.ropePressed = rope.ropeHeld = true;
            rope.aim = player.centre() + sf::Vector2f(std::cos(t.angle), std::sin(t.angle)) * 200.f;
            if (!player.canShootRope() || !player.ropeTarget(rope.aim)) return {Out::Missed, {}};
            for (int i = 0; i < int(THROW_WAIT / DT) && !player.getRope(); ++i) {
                if (auto o = step(rope)) return {*o, player.getPosition()};
                rope.ropePressed = false;
            }
            if (!player.getRope() || !player.canTie()) return {Out::Missed, {}};
            rope.tiePressed = true;
            if (auto o = step(rope)) return {*o, player.getPosition()};
            if (!player.isTied()) return {Out::Missed, {}};
            const sf::Vector2f hook = player.tiedLine().front(), knot = player.tiedLine().back();
            PlayerInput onto;
            onto.moveX = t.onto;
            onto.moveY = 1.f;
            onto.jumpPressed = onto.jumpHeld = true;
            for (int i = 0; i < int(1.2f / DT) && !player.isHanging(); ++i) {
                if (auto o = step(onto)) return {*o, player.getPosition()};
                onto.jumpPressed = false;
                if (i > 2 && player.isGrounded()) return {Out::Missed, {}};
            }
            if (!player.isHanging()) return {Out::Missed, {}};
            PlayerInput along;
            along.moveX = (hook.x > knot.x ? 1.f : -1.f) * t.along;
            along.moveY = t.along;
            for (int i = 0; i < int(t.time / DT) && player.isHanging(); ++i) {
                if (auto o = step(along)) return {*o, player.getPosition()};
            }
            PlayerInput off;
            off.moveX = t.steer;
            off.jumpPressed = off.jumpHeld = true;
            for (int i = 0; i < int(0.3f / DT); ++i) {
                if (auto o = step(off)) return {*o, player.getPosition()};
                off.jumpPressed = false;
            }
            return settle(t.steer, true);
        }

        Result drift(sf::Vector2f spot, const Drift& d)
        {
            start(spot);
            PlayerInput in;
            in.moveX = d.push.x;
            in.moveY = d.push.y;
            for (int i = 0; i < int(d.time / DT); ++i) {
                if (auto o = step(in)) return {*o, player.getPosition()};
            }
            return settle(0.f, true);
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

        // Every try from a spot starts with the room as it was: icicles up, blocks whole, rings at
        // the start, and with as much breath as there was at the spot.
        float air = 1.f;
        void start(sf::Vector2f spot)
        {
            level->reset();
            player.respawn(spot);
            player.setAirLeft(air);
        }

        std::optional<Out> step(const PlayerInput& in)
        {
            player.handleInput(in);
            level->update(DT, player.hitbox());
            player.update(DT);
            // What the game does with carts: in one, walking into it; out of one, at the end of the line.
            if (player.isRiding() && player.isGrounded() && level->isAtCartStop(player.getPosition(), Player::HEIGHT, Player::WIDTH)) {
                player.stopRiding();
                if (goal.stop) return Out::Reached;
            }
            if (!player.isRiding()) {
                if (const auto cart = level->cartAt(player.getPosition(), Player::HEIGHT, Player::WIDTH); cart && player.isGrounded()) {
                    if (goal.cart) return Out::Reached;
                }
            }
            if (player.hasCrashed()) return Out::Died;
            if (goal.at && player.isGrounded()) {
                const sf::FloatRect b = level->bounds();
                const int rows = int(b.height / Level::TILE_SIZE);
                const int column = int((player.getPosition().x + Player::WIDTH * 0.5f) / Level::TILE_SIZE);
                const int row = rows - 1 - int((player.getPosition().y - b.top) / Level::TILE_SIZE);
                if (column == goal.at->x && row == goal.at->y) return Out::Reached;
            }
            if (goal.plate && level->gatesHeld()) return Out::Reached;
            bool tugged = false;
            for (const PlayerEvent& e : player.takeEvents()) {
                tugged |= e.type == PlayerEvent::Type::LeverTugged || e.type == PlayerEvent::Type::LampTugged;
            }
            ++steps;
            if (goal.tug && tugged) return Out::Reached;
            if (goal.pickup && level->pickupAt(player.getPosition(), Player::HEIGHT, Player::WIDTH)) return Out::Reached;
            if (goal.gem >= 0 && level->gemAt(player.getPosition(), Player::HEIGHT, Player::WIDTH) == size_t(goal.gem)) return Out::Reached;
            if (player.isTouchingDanger() || player.isOutOfAir() || player.getPosition().y < level->bounds().top - 64.f) return Out::Died;
            if (auto side = level->exitThrough(player.centre())) {
                bool wanted = !goal.any() && (!goal.side || *side == *goal.side);
                if (wanted && goal.door >= 0) {
                    // Which door: where along the side you went through it, from the bottom or the left.
                    const bool upright = *side == Level::Side::Left || *side == Level::Side::Right;
                    const float along = upright ? player.centre().y - level->bounds().top : player.centre().x - level->bounds().left;
                    const int tile = int(along / Level::TILE_SIZE);
                    wanted = tile >= goal.door && tile < goal.door + 6;
                }
                return wanted ? Out::Reached : Out::Died;
            }
            return std::nullopt;
        }

    private:
        // Steers until landing, then waits to come to rest. With no gravity, until drifting to a stop.
        Result settle(float steer, bool airborne)
        {
            if (player.isFloating() || (player.isUnderwater() && !player.getRope())) {
                // Drifting, or afloat under the water (up against a roof, or rising): wherever it
                // comes to rest, holding its breath meanwhile.
                PlayerInput none;
                const float wait = player.isFloating() ? 9.f : 5.f; // drifting on, nothing slows you
                for (int i = 0; i < int(wait / DT); ++i) {
                    if (auto o = step(none)) return {*o, player.getPosition()};
                    if (i > 3 && !player.getRope() && magnitude(player.getVelocity()) < 3.f) return {Out::Landed, player.getPosition(), player.airLeft()};
                    if (player.isGrounded() && !player.isUnderwater()) break; // out of it, onto something
                }
                if (!player.isGrounded()) return {Out::Timeout, {}};
            }
            PlayerInput in;
            in.moveX = steer;
            for (int i = 0; i < int(4.f / DT); ++i) {
                // A bounce mushroom isn't somewhere to land: it throws you on.
                const bool grounded = player.isGrounded() && !player.getRope() && !level->isOnBounce(player.getPosition(), Player::WIDTH);
                if (grounded && (airborne || i > 0)) break;
                airborne |= !grounded;
                if (auto o = step(in)) return {*o, player.getPosition()};
                if (i == int(4.f / DT) - 1) return {Out::Timeout, {}};
            }
            for (int i = 0; i < int(0.3f / DT); ++i) {
                if (auto o = step({})) return {*o, player.getPosition()};
            }
            if (!player.isGrounded()) return {Out::Timeout, {}};
            return {Out::Landed, player.getPosition(), player.airLeft()};
        }

    };

    std::string describe(const Jump& j)
    {
        char b[160];
        std::snprintf(b, sizeof b, "%s run=%+.0f for %.2fs, jump %.2fs steering %+.0f%s", j.drop ? "drop" : "jump", j.run, j.runTime, j.hold, j.steer,
                      j.airJumpAt >= 0.f ? (", jump again after " + std::to_string(j.airJumpAt).substr(0, 4) + "s").c_str() : "");
        if (j.wait > 0.f) return "after " + std::to_string(j.wait).substr(0, 3) + "s, " + b;
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
        if (s.wait > 0.f) text = "after " + std::to_string(s.wait).substr(0, 3) + "s, " + text;
        if (s.outFirst > 0.f) {
            std::snprintf(b, sizeof b, ", letting it out for %.1fs first", s.outFirst);
            text += b;
        }
        if (s.again >= 0.f) {
            std::snprintf(b, sizeof b, ", then the rope again in the air at %.0f deg for %.2fs", s.again * 57.2958f, s.holdAgain);
            text += b;
        }
        return text;
    }
}

namespace
{
    // A ride in a mine cart from `start`, rolling `dir`-wards: it goes where it goes, and all there is
    // to choose is when to jump, and for how long. Every choice, a few steps apart, breadth first.
    int ride(Bot& bot, sf::Vector2f start, int dir, const std::string& name)
    {
        struct Press
        {
            int at, hold; // steps
        };
        struct Plan
        {
            std::vector<Press> presses;
            int next; // the step to choose at
        };
        constexpr int EVERY = 3;
        const int longest = int(40.f / DT);
        const int holds[] = {int(0.06f / DT), int(0.14f / DT), int(0.3f / DT), int(0.5f / DT)};
        // Plays a plan from the start up to step `until`: how it ended, if it has.
        auto replay = [&](const Plan& plan, int until) -> std::optional<Out> {
            bot.start(start);
            bot.player.startRiding(dir);
            size_t next = 0;
            int holding = 0;
            for (int i = 0; i < until; ++i) {
                PlayerInput in;
                if (next < plan.presses.size() && plan.presses[next].at == i) {
                    in.jumpPressed = true;
                    holding = plan.presses[next++].hold;
                }
                in.jumpHeld = holding > 0;
                holding = std::max(0, holding - 1);
                if (auto out = bot.step(in)) return out;
                if (!bot.player.isRiding()) return Out::Landed; // stopped, not where it was wanted
            }
            return std::nullopt;
        };
        std::deque<Plan> open{{{}, EVERY}};
        std::set<std::tuple<int, int, int, bool, bool>> seen;
        float furthest = start.x;
        while (!open.empty()) {
            const Plan plan = open.front();
            open.pop_front();
            const auto out = replay(plan, plan.next);
            furthest = dir > 0 ? std::max(furthest, bot.player.getPosition().x) : std::min(furthest, bot.player.getPosition().x);
            if (out == Out::Reached) {
                std::printf("%s: REACHED (riding, %lld steps)\n", name.c_str(), bot.steps);
                for (const Press& p : plan.presses) std::printf("  jump after %.2fs, held %.2fs\n", p.at * DT, p.hold * DT);
                std::fflush(stdout);
                std::_Exit(0);
            }
            if (out || plan.next > longest) continue;
            const Player& p = bot.player;
            const auto state = std::make_tuple(plan.next, int(p.getPosition().y), int(std::floor(p.getVelocity().y / 15.f)), p.isGrounded(), p.hasAirJump());
            if (!seen.insert(state).second) continue;
            open.push_back({plan.presses, plan.next + EVERY});
            if (p.isGrounded() || p.hasAirJump()) {
                for (int hold : holds) {
                    Plan jump = plan;
                    jump.presses.push_back({plan.next, hold});
                    jump.next = plan.next + EVERY;
                    open.push_back(jump);
                }
            }
        }
        std::printf("%s: NOT REACHED (riding, got to column %d, %lld steps)\n", name.c_str(), int(furthest / Level::TILE_SIZE), bot.steps);
        std::fflush(stdout);
        std::_Exit(1);
    }

    const std::set<std::string> ALL_ABILITIES = {"rope", "long_rope", "wall_climb", "air_throw", "double_jump"};

    void giveAbilities(Player& player, const std::set<std::string>& abilities)
    {
        player.setBreathScale(abilities.count("diver") ? physics().divingAir : 1.f);
        player.setHasRope(abilities.count("rope") > 0);
        player.setRopeRange(abilities.count("long_rope") ? physics().longRopeRange : physics().ropeRange);
        player.setCanClimb(abilities.count("wall_climb") > 0);
        player.setCanThrowInAir(abilities.count("air_throw") > 0);
        player.setCanDoubleJump(abilities.count("double_jump") > 0);
        player.setCanTie(abilities.count("tie_off") > 0);
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
    int riding = 0;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--start" && i + 2 < argc) {
            startTile = sf::Vector2i(std::atoi(argv[i + 1]), std::atoi(argv[i + 2]));
            i += 2;
        } else if (a == "--goal" && i + 1 < argc) {
            const std::string g = argv[++i];
            if (g == "pickup") goal.pickup = true;
            else if (g == "tug") goal.tug = true;
            else if (g == "cart") goal.cart = true;
            else if (g == "stop") goal.stop = true;
            else if (g == "plate") goal.plate = true;
            else if (g == "at" && i + 2 < argc) {
                goal.at = sf::Vector2i(std::atoi(argv[i + 1]), std::atoi(argv[i + 2]));
                i += 2;
            }
            else if (g == "gem" && i + 1 < argc) goal.gem = std::atoi(argv[++i]);
            else {
                // "L", or "L:21" for the left door starting 21 up.
                goal.side = g[0] == 'L' ? Level::Side::Left : g[0] == 'R' ? Level::Side::Right : g[0] == 'T' ? Level::Side::Top : Level::Side::Bottom;
                if (g.size() > 2 && g[1] == ':') goal.door = std::atoi(g.c_str() + 2);
            }
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
        } else if (a == "--riding" && i + 1 < argc) {
            riding = std::atoi(argv[++i]) < 0 ? -1 : 1;
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
        for (float runTime : {0.f, 0.04f, 0.08f, 0.12f, 0.18f, 0.25f, 0.35f, 0.45f, 0.6f}) {
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
        // With no gravity: every way round, and nothing but how long it's reeled in on or held.
        if (!rope || level->isSpace()) continue;
        if (jumpFirst && !airThrow) continue; // thrown standing only, until the throwing hand
        for (int deg = 25; deg <= 155; deg += 10) {
            for (float push : {-1.f, 1.f}) {
                for (bool reel : {false, true}) {
                    for (float hold : {0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.4f, 1.6f, 1.8f, 2.0f, 2.2f}) {
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

    if (rope && level->isSpace()) {
        for (int deg = 0; deg < 360; deg += 10) {
            for (bool reel : {false, true}) {
                for (float hold : {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.2f, 1.4f, 1.7f, 2.0f, 2.5f, 3.f}) {
                    swings.push_back({false, deg / 57.2958f, 1.f, reel, hold, false, 0.f, false});
                }
            }
        }
    }
    // With stones about: hooking one, lowering yourself on it off a ledge first, then reeling it in.
    if (rope && !level->stones().empty()) {
        for (int deg = 0; deg < 360; deg += 10) {
            for (float push : {-1.f, 1.f}) {
                for (float out : {0.3f, 0.6f, 1.0f, 1.5f}) {
                    for (float hold : {0.5f, 1.0f, 2.0f, 3.0f}) {
                        Swing lower{false, deg / 57.2958f, push, true, hold, false, 0.f, false};
                        lower.outFirst = out;
                        swings.push_back(lower);
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
    // With rings going round, or fires that come and go, the same swing (or run, or jump) a moment
    // later is another.
    const bool breathing = std::any_of(level->fires().begin(), level->fires().end(), [](const Fire& f) { return f.period > 0.f; });
    if (breathing) {
        const size_t count = jumps.size();
        for (float wait : {0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f}) {
            for (size_t i = 0; i < count; ++i) {
                Jump later = jumps[i];
                later.wait = wait;
                jumps.push_back(later);
            }
        }
    }
    if (!level->rings().empty() || breathing) {
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
    // With no gravity, or under water: pushes (or strokes) every way, short and long.
    bool water = false;
    for (int x = int(b.left / Level::TILE_SIZE); x < int((b.left + b.width) / Level::TILE_SIZE) && !water; ++x) {
        for (int y = int(b.top / Level::TILE_SIZE); y < int((b.top + b.height) / Level::TILE_SIZE) && !water; ++y) water = level->isWater(x, y);
    }
    std::vector<Drift> drifts;
    if (level->isSpace() || water) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                // Floating, clambering along the rock, slowly.
                if (level->isSpace()) {
                    for (float time : {0.3f, 0.6f, 1.f, 1.6f, 2.5f, 4.f}) drifts.push_back({sf::Vector2f(float(dx), float(dy)), time});
                    continue;
                }
                for (float time : {0.12f, 0.25f, 0.4f, 0.6f, 0.9f, 1.3f, 1.8f}) drifts.push_back({sf::Vector2f(float(dx), float(dy)), time});
            }
        }
        if (level->isSpace()) jumps.clear(); // nothing to jump off
    }
    // With the knot: tied off every way but steeply down, and onto the line, along it and off.
    std::vector<Tie> ties;
    if (rope && abilities.count("tie_off")) {
        for (int deg = -60; deg <= 240; deg += 10) {
            for (float onto : {-1.f, 1.f}) {
                for (float along : {1.f, -1.f}) {
                    for (float time : {0.6f, 1.5f, 3.f}) {
                        for (float steer : {-1.f, 1.f}) ties.push_back({deg / 57.2958f, onto, along, time, steer});
                    }
                }
            }
        }
    }
    std::vector<Climb> climbing;
    for (float dir : {-1.f, 1.f}) {
        if (!climbs || level->isSpace()) continue;
        for (float time : {0.f, 0.4f, 0.8f, 1.2f, 1.7f}) {
            for (float steer : {-dir, 0.f, dir}) climbing.push_back({dir, time, true, steer});
            for (float steer : {-dir, 0.f}) climbing.push_back({dir, time, false, steer});
        }
        climbing.push_back({dir, 5.f, false, 0.f}); // as high as it goes: over the top
        // Up a chimney, wall to wall, then as high as it goes.
        for (int leaps : {2, 4, 6, 8}) climbing.push_back({dir, 5.f, false, 0.f, leaps});
    }

    if (riding) return ride(bot, start, riding, roomArg);

    // Best-first, furthest from the start first: where the goal is isn't known.
    struct Node
    {
        sf::Vector2f pos;
        int parent;
        std::string how;
        float air = 1.f; // breath left there: a spot got to with more of it is worth trying again
    };
    std::vector<Node> nodes;
    std::map<std::pair<int, int>, int> seen;
    // Standing, the height is exact; floating, anywhere near is the same spot.
    const bool space = level->isSpace();
    auto key = [space](sf::Vector2f p) {
        return space ? std::make_pair(int(std::floor(p.x / 5.f)), int(std::floor(p.y / 5.f))) : std::make_pair(int(std::floor(p.x / 6.f)), int(p.y));
    };
    auto priority = [&](int i) { return std::abs(nodes[i].pos.x - start.x) + std::abs(nodes[i].pos.y - start.y); };
    std::priority_queue<std::pair<float, int>> open;

    player.respawn(start);
    PlayerInput none;
    for (int i = 0; i < int(1.f / DT); ++i) {
        player.handleInput(none);
        player.update(DT);
    }
    nodes.push_back({player.getPosition(), -1, "start", player.airLeft()});
    seen[key(nodes[0].pos)] = 0;
    open.push({priority(0), 0});

    int found = -1;
    std::string foundHow;
    while (!open.empty() && found < 0) {
        const int i = open.top().second;
        open.pop();
        const sf::Vector2f spot = nodes[i].pos;
        bot.air = nodes[i].air;
        auto handle = [&](Result r, const std::string& how) {
            if (r.out == Out::Reached && !explore) {
                found = i;
                foundHow = how;
                return true;
            }
            if (r.out != Out::Landed) return false;
            if (const auto known = seen.find(key(r.pos)); known != seen.end() && nodes[known->second].air >= r.air - 0.02f) return false;
            seen[key(r.pos)] = static_cast<int>(nodes.size());
            nodes.push_back({r.pos, i, how, r.air});
            open.push({priority(static_cast<int>(nodes.size()) - 1), static_cast<int>(nodes.size()) - 1});
            return false;
        };
        bool done = false;
        for (const Jump& j : jumps) {
            if ((done = handle(bot.jump(spot, j), describe(j)))) break;
        }
        if (done) break;
        std::set<int> tieMisses; // once a tie misses at an angle, it will however it goes on
        for (const Tie& t : ties) {
            const int deg = int(std::round(t.angle * 57.2958f));
            if (tieMisses.count(deg)) continue;
            const Result r = bot.tie(spot, t);
            if (r.out == Out::Missed) {
                if (!bot.player.isTied()) tieMisses.insert(deg);
                continue;
            }
            char how[160];
            std::snprintf(how, sizeof how, "rope at %d deg, tie it off, onto the line %+.0f, along it %s for %.1fs, jump off steering %+.0f", deg, t.onto,
                          t.along > 0.f ? "towards the hook" : "away from it", t.time, t.steer);
            if ((done = handle(r, how))) break;
        }
        if (done) break;
        for (const Drift& d : drifts) {
            char how[80];
            std::snprintf(how, sizeof how, "push %+.0f,%+.0f for %.2fs, drift", d.push.x, d.push.y, d.time);
            if (level->isSpace()) std::snprintf(how, sizeof how, "clamber along the rock %+.0f,%+.0f for %.1fs", d.push.x, d.push.y, d.time);
            if ((done = handle(bot.drift(spot, d), how))) break;
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
