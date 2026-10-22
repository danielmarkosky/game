// Finds out whether a room can be got through, by trying moves with the real player physics: a
// search over the spots the player can stand on, trying runs, jumps and rope swings from each.
// It doesn't play like a person, but if it gets through, a person can too; if it can't, the room
// is probably too hard, or impossible.
//
// usage: levelbot <room> [--start COL ROW] [--goal L|R|T|B | --goal pickup | --goal gem N]
//                        [--abilities a,b,...] [--explore]
//   <room>       a room's identifier in assets/world.ldtk, or a text room file
//   --start      where to start: the tile to stand in, column and row from the top-left
//                (default: the room's Start)
//   --goal       which way out counts (default: any), or touching a pickup, or gem N
//   --abilities  the ones the player has (default: all). "none" for none.
//   --explore    don't stop at the goal: list every spot that can be stood on
//   --open       with the room's gates open, as if its lever had been pulled
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
    };

    // A run, then maybe a jump, held for a while, steering in the air, maybe jumping again.
    struct Jump
    {
        float run, runTime, hold, steer;
        bool drop;        // hold down first, to drop through a platform
        float airJumpAt;  // < 0: no second jump
    };

    // Maybe a jump first, then the rope shot at an angle, swung on for a while, let go of.
    struct Swing
    {
        bool jumpFirst;
        float angle, push;
        bool reel;
        float hold;
        bool jumpOff;
        float steer;
        bool airJump;
    };

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
            player.respawn(spot);
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
            player.respawn(spot);
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
            rope.moveX = s.push;
            rope.moveY = s.reel ? 1.f : 0.f;
            rope.ropePressed = rope.ropeHeld = true;
            rope.jumpHeld = s.jumpFirst;
            rope.aim = player.centre() + sf::Vector2f(std::cos(s.angle), std::sin(s.angle)) * 200.f;
            player.handleInput(rope);
            if (!player.getRope()) return {Out::Missed, {}};
            rope.ropePressed = false;
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
            return settle(s.steer, true);
        }

    private:
        std::optional<Out> step(const PlayerInput& in)
        {
            player.handleInput(in);
            player.update(DT);
            player.takeEvents();
            ++steps;
            if (goal.pickup && level->pickupAt(player.getPosition(), Player::HEIGHT, Player::WIDTH)) return Out::Reached;
            if (goal.gem >= 0 && level->gemAt(player.getPosition(), Player::HEIGHT, Player::WIDTH) == size_t(goal.gem)) return Out::Reached;
            if (player.isTouchingDanger() || player.getPosition().y < level->bounds().top - 64.f) return Out::Died;
            if (auto side = level->exitThrough(player.centre())) {
                const bool wanted = !goal.pickup && goal.gem < 0 && (!goal.side || *side == *goal.side);
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

    std::string describe(const Swing& s)
    {
        char b[160];
        std::snprintf(b, sizeof b, "rope%s at %.0f deg, push %+.0f%s for %.2fs, %s steering %+.0f%s", s.jumpFirst ? " mid-jump" : "",
                      s.angle * 57.2958f, s.push, s.reel ? " reeling in" : "", s.hold, s.jumpOff ? "jump off" : "let go", s.steer,
                      s.airJump ? ", jump again" : "");
        return b;
    }
}

int main(int argc, char** argv)
{
    // Moving as in the game.
    for (const std::string& problem : loadPhysics(config::assetPath("physics.txt"))) std::cerr << "physics.txt: " << problem << "\n";
    if (argc < 2) {
        std::cerr << "usage: levelbot <room> [--start COL ROW] [--goal L|R|T|B|pickup|gem N] [--abilities a,b] [--explore]\n";
        return 2;
    }
    const std::string roomArg = argv[1];
    std::optional<sf::Vector2i> startTile;
    Goal goal;
    std::set<std::string> abilities = {"climbing_pick", "long_rope", "double_jump"};
    bool explore = false, openGates = false;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--start" && i + 2 < argc) {
            startTile = sf::Vector2i(std::atoi(argv[i + 1]), std::atoi(argv[i + 2]));
            i += 2;
        } else if (a == "--goal" && i + 1 < argc) {
            const std::string g = argv[++i];
            if (g == "pickup") goal.pickup = true;
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
    player.setCanHookRock(abilities.count("climbing_pick") > 0);
    player.setRopeRange(abilities.count("long_rope") ? physics().longRopeRange : physics().ropeRange);
    player.setCanDoubleJump(abilities.count("double_jump") > 0);
    const bool doubleJump = abilities.count("double_jump") > 0;

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
        for (int deg = 25; deg <= 155; deg += 10) {
            for (float push : {-1.f, 1.f}) {
                for (bool reel : {false, true}) {
                    for (float hold : {0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.8f, 2.2f}) {
                        for (bool jumpOff : {false, true}) {
                            for (float steer : {push, 0.f, -push}) {
                                swings.push_back({jumpFirst, deg / 57.2958f, push, reel, hold, jumpOff, steer, false});
                                if (doubleJump && steer == push) swings.push_back({jumpFirst, deg / 57.2958f, push, reel, hold, jumpOff, steer, true});
                            }
                        }
                    }
                }
            }
        }
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
        // Once the rope misses at an angle from here, it'll miss whatever else is tried with it.
        std::set<std::pair<bool, int>> misses;
        for (const Swing& s : swings) {
            const auto aim = std::make_pair(s.jumpFirst, int(std::round(s.angle * 57.2958f)));
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
