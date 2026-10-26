#pragma once

#include "src/Theme.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Something lying in a level that gives the player an ability when touched.
struct Pickup
{
    std::string ability;   // e.g. "long_rope"
    sf::Vector2f position; // bottom-left of its tile
};

// Someone to talk to.
struct Npc
{
    std::string dialogue;  // which file in assets/dialogue/ has what they say
    std::string look;      // one of Player::LOOKS
    sf::Vector2f position; // bottom-left of their tile: they stand on its bottom edge
    bool sitting = false;  // sitting on the ground (by a fire, say), not standing
    // Only there once this fact holds (an ability or a story flag), and until that one does: "" for always.
    std::string needs, until;
};

// Scenery (see src/Props.h): `kind` is its name in props.txt.
struct Prop
{
    std::string kind;
    sf::Vector2f position; // bottom-left of its tile
    // Until this fact holds, drawn as "<kind>_off" instead (a beacon not lit yet), or not at all
    // if there's none. "" for always.
    std::string needs;
};

// A mine cart standing on the rails: walking into it gets you in, and it rolls off `dir`-wards
// (1 right, -1 left). It won't budge until every fact in `needs` holds (separated by commas).
struct Cart
{
    sf::Vector2f position; // bottom-left of its tile
    int dir = 1;
    std::string needs;
};

// Where a timed run of a route starts or finishes: `route` is its name, one of assets/regions.txt.
struct Trial
{
    std::string route;
    bool finish = false;
    sf::Vector2f position; // bottom-left of its tile
};

// A hook ring sliding to and fro along a track: the rope catches it, and it carries the rope along.
struct Ring
{
    sf::Vector2f from, to; // its middle at each end, world pixels
    float period;          // seconds there and back
};

// A lantern hanging in the dark: the rope lights it.
struct Lamp
{
    sf::Vector2f position; // bottom-left of its tile; it hangs from the tile's top
    bool lit = false;
};

// An icicle over a drop: it shakes when you go under it, then falls, then grows back.
struct Icicle
{
    enum class State { Hanging, Shaking, Falling, Gone };
    sf::Vector2f home;     // its tip when hanging: the top of its tile, middle
    sf::Vector2f tip;
    float speed = 0.f;     // falling, px/s
    State state = State::Hanging;
    float timer = 0.f;
};

// Something that happened in the room this step that can be heard: where, and what.
struct LevelSound
{
    enum class Kind { Crack, Crumble, CloudFade, IcicleShake, IcicleShatter };
    Kind kind;
    sf::Vector2f position;
};

// What a rope thrown along a line meets first: rock it bites (or a hook block), slick stone it
// bounces off, a moving ring it catches, or a lever or lamp it tugs and comes back from.
struct RopeHit
{
    enum class Kind { Rock, Slick, Ring, Lever, Lamp };
    Kind kind;
    sf::Vector2f point;
    int index = -1;        // of the ring, lever or lamp
    float distance = 0.f;
};

class Level
{
public:
    static constexpr float TILE_SIZE = 8; // 8x8 tiles
    // Tile values, as in the LDtk project's Tiles layer (see tools/rooms.py).
    static constexpr int TILE_METAL = 1, TILE_ROCK = 2, TILE_CRYSTAL = 3, TILE_SPIKES = 4, TILE_PLATFORM = 5,
                         TILE_GATE = 6, TILE_SLICK = 7, TILE_ICE = 8, TILE_CRUMBLE = 9, TILE_CLOUD = 10,
                         TILE_WATER = 11, TILE_BOUNCE = 12, TILE_HIDDEN = 13, TILE_RAIL = 14;

    // The edges of a level. Going through a gap in one takes you into the room on the other side,
    // if there is one. Falling out of the bottom with no room below kills you.
    enum class Side { Left, Right, Top, Bottom };

    // A small bordered test room.
    Level();
    Level(const std::unordered_map<int, std::unordered_map<int, int>>& tiles);

    // Loads a level from a text file. See assets/levels/README.md for the format.
    // Throws std::runtime_error if the file can't be read or has no start position.
    static std::shared_ptr<Level> loadFromFile(const std::string& path);
    static std::shared_ptr<Level> loadFromString(const std::string& text);
    // A level from text rows like a level file's, without the start check: for building one from
    // the LDtk project, which then adds the entities with the functions below. Tile positions
    // count from the bottom left, like everywhere else.
    static std::shared_ptr<Level> fromRows(const std::vector<std::string>& rows);
    void setStart(sf::Vector2i tile);
    void addCheckpoint(sf::Vector2i tile);
    void setGoal(sf::Vector2i tile);
    void addPickup(const std::string& ability, sf::Vector2i tile);
    void addGem(sf::Vector2i tile);
    void addNpc(const Npc& npc, sf::Vector2i tile);
    void addNpc(const std::string& dialogue, const std::string& look, sf::Vector2i tile, bool sitting = false);
    void addBench(sf::Vector2i tile);
    void addLever(sf::Vector2i tile);
    void addProp(const std::string& kind, sf::Vector2i tile, const std::string& needs = "");
    void addIcicle(sf::Vector2i tile);  // hanging from the top of the tile
    void addUpdraft(sf::Vector2i tile); // rising from the tile, two tiles wide, up to the first rock
    void addRing(sf::Vector2i tile, sf::Vector2i to, float period); // from the tile's middle to `to`'s
    void addLamp(sf::Vector2i tile);
    void addCart(sf::Vector2i tile, int dir = 1, const std::string& needs = "");
    void addCartStop(sf::Vector2i tile); // the end of the line: a cart riding into it stops, and you get out
    void addTrial(const std::string& route, bool finish, sf::Vector2i tile);
    // The whole room: wind blowing (px/s each second, + to the right) on anything in the air;
    // dark, lit only by what glows in it; and with no gravity, everything floating.
    void setWind(float wind);
    void setDark(bool dark);
    void setSpace(bool space);
    float wind() const;
    bool isDark() const;
    bool isSpace() const;

    // What moves or changes: icicles, crumbling blocks and clouds (by where the player is), rings
    // (by the time). Once a physics step, for the room the player is in. reset() puts it all back.
    void update(float dt, sf::FloatRect player);
    void reset();
    float time() const;
    // What's happened since the last call that can be heard (see LevelSound).
    std::vector<LevelSound> takeSounds();

    const std::vector<Ring>& rings() const;
    sf::Vector2f ringPosition(size_t ring) const; // now
    const std::vector<Lamp>& lamps() const;
    void setLampLit(size_t lamp, bool lit);
    const std::vector<Icicle>& icicles() const;
    // How hard an updraft lifts a point: 0 outside them, 1 inside.
    float updraftAt(sf::Vector2f point) const;
    const std::vector<sf::FloatRect>& updrafts() const;
    // Standing on ice: a box of `width` with its bottom at `feet`.
    bool isOnIce(sf::Vector2f feet, float width) const;
    bool isIce(int x, int y) const;
    // Water ('w'): not solid, you swim in it. How deep a point is under the surface of the water
    // it's in, in pixels (the top of the highest water tile straight above it), if it's in any.
    bool isWater(int x, int y) const;
    std::optional<float> depthInWater(sf::Vector2f point) const;
    // The water, see-through, drawn over the player so what's under the surface is under it.
    void drawWater(sf::RenderTarget& target, sf::Vector2f centr, float time) const;
    // Along the line from `from` towards `to`, at most `range` away: what the rope meets first.
    std::optional<RopeHit> ropeHit(sf::Vector2f from, sf::Vector2f to, float range) const;
    // The things that move or glow, drawn after the tiles.
    void drawMoving(sf::RenderTarget& target, sf::Vector2f centr, float time) const;
    // Hidden passages ('?'): they look like rock, but they're not there. Drawn over the player, and
    // fading away around them while they're in one (see update).
    void drawHidden(sf::RenderTarget& target, sf::Vector2f centr, sf::Vector2f player) const;

    // Mine carts waiting, and where the line ends. `ready` says whether a cart's facts hold.
    const std::vector<Cart>& carts() const;
    std::optional<size_t> cartAt(sf::Vector2f pos, float height, float width) const;
    const std::vector<sf::Vector2f>& cartStops() const;
    bool isAtCartStop(sf::Vector2f pos, float height, float width) const;
    void drawCarts(sf::RenderTarget& target, sf::Vector2f centr, const std::function<bool(const Cart&)>& ready) const;
    // A cart with someone in it, its wheels on the ground at `bottom` (the middle of its bottom).
    void drawRiddenCart(sf::RenderTarget& target, sf::Vector2f centr, sf::Vector2f bottom, float time) const;
    // The start and finish of timed runs: the posts, drawn once you have the stopwatch.
    const std::vector<Trial>& trials() const;
    const Trial* trialAt(sf::Vector2f pos, float height, float width) const;
    void drawTrials(sf::RenderTarget& target, sf::Vector2f centr, float time) const;

    // The tiles are drawn with the theme's tiles.png. New levels use Theme::DEFAULT.
    void setTheme(std::shared_ptr<const Theme> theme);
    const Theme& theme() const;

    void draw(sf::RenderTarget& target, sf::Vector2f centr) const;
    void drawMarkers(sf::RenderTarget& target, sf::Vector2f centr, std::optional<sf::Vector2f> activeCheckpoint, float time) const;
    // The pickups whose ability `owned` says the player doesn't have yet, bobbing up and down.
    void drawPickups(sf::RenderTarget& target, sf::Vector2f centr, float time, const std::function<bool(const std::string&)>& owned) const;
    void drawLevers(sf::RenderTarget& target, sf::Vector2f centr) const;
    // The scenery, behind everything but the tiles. Animated props move with `time`; `holds` says
    // whether a prop's `needs` holds.
    void drawProps(sf::RenderTarget& target, sf::Vector2f centr, float time, const std::function<bool(const std::string&)>& holds) const;
    // The benches, the one at `glowing` (if any) lit up: where the game was saved last.
    void drawBenches(sf::RenderTarget& target, sf::Vector2f centr, std::optional<size_t> glowing, float time) const;
    // The gems, the ones `collected` says were found already as faint ghosts.
    void drawGems(sf::RenderTarget& target, sf::Vector2f centr, float time, const std::function<bool(size_t)>& collected) const;

    int tileAt(int x, int y) const;
    bool isSolid(int x, int y) const;
    bool isHookBlock(int x, int y) const; // '%'
    bool isSlick(int x, int y) const;     // 'o': solid, but the rope slides off it
    bool isBounce(int x, int y) const;    // 'm': a bounce mushroom, solid, throwing you up when you land on it
    bool isHidden(int x, int y) const;    // '?': looks like rock, but isn't there
    bool isRail(int x, int y) const;      // 't': rails, a one-way platform carts run along
    // True if a box of `width` with its bottom at `feet` stands exactly on top of a bounce mushroom.
    bool isOnBounce(sf::Vector2f feet, float width) const;
    bool isDangerous(int x, int y) const;
    // One-way platforms: you can jump up through them, but land on their top.
    bool isPlatform(int x, int y) const;
    // True if a box of `width` with its bottom at `feet` stands exactly on top of a platform.
    bool isOnPlatform(sf::Vector2f feet, float width) const;
    bool isEmptySpace(sf::Vector2f pos, float height, float width) const;
    // True if the box touches the pointy part of a spike (the half of the tile next to its rock).
    bool isDangerousSpace(sf::Vector2f pos, float height, float width) const;

    // Respawn position of the checkpoint the box touches, if any.
    std::optional<sf::Vector2f> checkpointAt(sf::Vector2f pos, float height, float width) const;
    bool isGoal(sf::Vector2f pos, float height, float width) const;
    const std::vector<Pickup>& pickups() const;
    // The pickup the box touches, if any.
    const Pickup* pickupAt(sf::Vector2f pos, float height, float width) const;
    // Gems are collectibles, nothing more. Each is the bottom-left of its tile.
    const std::vector<sf::Vector2f>& gems() const;
    // The index of the gem the box touches, if any.
    std::optional<size_t> gemAt(sf::Vector2f pos, float height, float width) const;
    const std::vector<Npc>& npcs() const;
    const std::vector<Prop>& props() const;
    // Benches to sit on, two tiles wide, each the bottom-left of its left tile.
    const std::vector<sf::Vector2f>& benches() const;
    // Gates ('=') are solid until a lever in the same room is pulled; then they all stay open.
    const std::vector<sf::Vector2f>& levers() const;
    void setGatesOpen(bool open);
    bool gatesOpen() const;
    static constexpr float BENCH_WIDTH = 2 * TILE_SIZE;
    // Which edge of the level `point` has gone past, if any.
    std::optional<Side> exitThrough(sf::Vector2f point) const;

    bool hasStart() const;
    sf::Vector2f getStartPos() const;
    const std::vector<sf::Vector2f>& checkpoints() const;
    // World-space rectangle covering every tile. `left`/`top` hold the minimum x/y, and since
    // world y points up, `top` is actually the bottom edge.
    sf::FloatRect bounds() const;

private:
    void loadTextures();
    void computeBounds();
    // A rock tile (or a hidden passage's, which looks like one), picked from its neighbours.
    void drawRock(sf::Sprite& sprite, sf::RenderTarget& target, sf::Vector2f centr, int x, int y) const;

    std::unordered_map<int, std::unordered_map<int, int>> m_tileMap;
    std::vector<sf::Vector2f> m_checkpoints;
    std::optional<sf::Vector2f> m_goal;
    std::vector<Pickup> m_pickups;
    std::vector<sf::Vector2f> m_gems;
    std::vector<Npc> m_npcs;
    std::vector<Prop> m_props;
    std::vector<sf::Vector2f> m_benches;
    std::vector<sf::Vector2f> m_levers;
    bool m_gatesOpen = false;
    std::vector<Cart> m_carts;
    std::vector<sf::Vector2f> m_cartStops;
    std::vector<Trial> m_trials;

    // What moves or changes (see update).
    struct Crumbling
    {
        float stood = 0.f; // seconds stood on (or near) it
        float gone = 0.f;  // seconds until it's back, > 0 while it's gone
    };
    std::unordered_map<long long, Crumbling> m_crumbling; // by tile, crumbling blocks and clouds
    Crumbling* crumbling(int x, int y);
    const Crumbling* crumbling(int x, int y) const;
    std::vector<Ring> m_rings;
    std::vector<Lamp> m_lamps;
    std::vector<Icicle> m_icicles;
    std::vector<LevelSound> m_sounds;
    std::vector<sf::FloatRect> m_updrafts;
    std::vector<sf::Vector2i> m_updraftTiles; // where they start: their tops are found once all the tiles are in
    float m_wind = 0.f;
    bool m_dark = false;
    bool m_space = false;
    float m_time = 0.f;
    float m_reveal = 0.f; // 0 to 1: how far the hidden passages have faded, with the player in one
    std::shared_ptr<const Theme> m_theme;
    sf::Texture m_objects;
    sf::Texture m_items;
    sf::Texture m_bench;
    std::optional<sf::Vector2f> m_startPos;
    sf::FloatRect m_bounds;
};
