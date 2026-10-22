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
    std::string look;      // one of Player::SKINS
    sf::Vector2f position; // bottom-left of their tile: they stand on its bottom edge
};

// Scenery (see src/Props.h): `kind` is its name in props.txt.
struct Prop
{
    std::string kind;
    sf::Vector2f position; // bottom-left of its tile
};

// Something to find for someone, lying in a room (see assets/quests.txt).
struct QuestItem
{
    std::string item;
    sf::Vector2f position; // bottom-left of its tile
};

// Where a race ends (see assets/quests.txt).
struct Finish
{
    std::string race;
    sf::Vector2f position; // bottom-left of its tile: the flag stands on its bottom
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
                         TILE_GATE = 6, TILE_SLICK = 7, TILE_ICE = 8, TILE_CRUMBLE = 9, TILE_CLOUD = 10;

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
    void addNpc(const std::string& dialogue, const std::string& look, sf::Vector2i tile);
    void addBench(sf::Vector2i tile);
    void addLever(sf::Vector2i tile);
    void addProp(const std::string& kind, sf::Vector2i tile);
    void addIcicle(sf::Vector2i tile);  // hanging from the top of the tile
    void addUpdraft(sf::Vector2i tile); // rising from the tile, two tiles wide, up to the first rock
    void addRing(sf::Vector2i tile, sf::Vector2i to, float period); // from the tile's middle to `to`'s
    void addLamp(sf::Vector2i tile);
    void addQuestItem(const std::string& item, sf::Vector2i tile);
    void addFinish(const std::string& race, sf::Vector2i tile);
    const std::vector<QuestItem>& questItems() const;
    const std::vector<Finish>& finishes() const;
    // The one the box touches, if any: an index into the lists above.
    std::optional<size_t> questItemAt(sf::FloatRect box) const;
    std::optional<size_t> finishAt(sf::FloatRect box) const;
    // Items not found yet (`found` says which are), bobbing; the races' finishing flags.
    void drawQuestItems(sf::RenderTarget& target, sf::Vector2f centr, float time, const std::function<bool(size_t)>& found) const;
    // The whole room: wind blowing (px/s each second, + to the right) on anything in the air; and
    // dark, lit only by what glows in it.
    void setWind(float wind);
    void setDark(bool dark);
    float wind() const;
    bool isDark() const;

    // What moves or changes: icicles, crumbling blocks and clouds (by where the player is), rings
    // (by the time). Once a physics step, for the room the player is in. reset() puts it all back.
    void update(float dt, sf::FloatRect player);
    void reset();
    float time() const;

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
    // Along the line from `from` towards `to`, at most `range` away: what the rope meets first.
    std::optional<RopeHit> ropeHit(sf::Vector2f from, sf::Vector2f to, float range) const;
    // The things that move or glow, drawn after the tiles.
    void drawMoving(sf::RenderTarget& target, sf::Vector2f centr, float time) const;

    // The tiles are drawn with the theme's tiles.png. New levels use Theme::DEFAULT.
    void setTheme(std::shared_ptr<const Theme> theme);
    const Theme& theme() const;

    void draw(sf::RenderTarget& target, sf::Vector2f centr) const;
    void drawMarkers(sf::RenderTarget& target, sf::Vector2f centr, std::optional<sf::Vector2f> activeCheckpoint, float time) const;
    // The pickups whose ability `owned` says the player doesn't have yet, bobbing up and down.
    void drawPickups(sf::RenderTarget& target, sf::Vector2f centr, float time, const std::function<bool(const std::string&)>& owned) const;
    void drawLevers(sf::RenderTarget& target, sf::Vector2f centr) const;
    // The scenery, behind everything but the tiles. Animated props move with `time`.
    void drawProps(sf::RenderTarget& target, sf::Vector2f centr, float time) const;
    // The benches, the one at `glowing` (if any) lit up: where the game was saved last.
    void drawBenches(sf::RenderTarget& target, sf::Vector2f centr, std::optional<size_t> glowing, float time) const;
    // The gems, the ones `collected` says were found already as faint ghosts.
    void drawGems(sf::RenderTarget& target, sf::Vector2f centr, float time, const std::function<bool(size_t)>& collected) const;

    int tileAt(int x, int y) const;
    bool isSolid(int x, int y) const;
    bool isHookBlock(int x, int y) const; // '%'
    bool isSlick(int x, int y) const;     // 'o': solid, but the rope slides off it
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
    std::vector<QuestItem> m_questItems;
    std::vector<Finish> m_finishes;
    std::vector<Icicle> m_icicles;
    std::vector<sf::FloatRect> m_updrafts;
    std::vector<sf::Vector2i> m_updraftTiles; // where they start: their tops are found once all the tiles are in
    float m_wind = 0.f;
    bool m_dark = false;
    float m_time = 0.f;
    std::shared_ptr<const Theme> m_theme;
    sf::Texture m_objects;
    sf::Texture m_items;
    sf::Texture m_bench;
    std::optional<sf::Vector2f> m_startPos;
    sf::FloatRect m_bounds;
};
