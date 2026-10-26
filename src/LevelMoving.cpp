// Level: what moves or changes in a room. Crumbling blocks and clouds that give way under you,
// icicles that fall, hook rings sliding on their tracks, updrafts, lamps, mine carts, hidden
// passages, wind, the dark and no gravity.
#include "Level.h"
#include "src/Config.h"
#include "src/Props.h"
#include "src/utils.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr float CRUMBLE_DELAY = 0.5f; // stood on this long, a crumbling block gives way...
    constexpr float CRUMBLE_BACK = 2.5f;  // ...and is back after this
    constexpr float CLOUD_DELAY = 0.8f;
    constexpr float CLOUD_BACK = 2.f;
    constexpr float ICICLE_SHAKE = 0.35f;  // shaking before it falls
    constexpr float ICICLE_GRAVITY = 900.f;
    constexpr float ICICLE_MAX_SPEED = 420.f;
    constexpr float ICICLE_BACK = 2.5f;    // grown back after this
    constexpr float ICICLE_SIGHT = 14.f;   // tiles: how far below it notices you
    constexpr float RING_RADIUS = 4.f;     // what the rope has to pass through to catch it
    constexpr float PI = 3.14159265f;

    long long tileKey(int x, int y)
    {
        return (static_cast<long long>(x) << 32) ^ static_cast<unsigned>(y);
    }

    bool inside(sf::FloatRect box, sf::Vector2f p)
    {
        return p.x >= box.left && p.x < box.left + box.width && p.y >= box.top && p.y < box.top + box.height;
    }

    void drawProp(sf::RenderTarget& target, const std::string& name, sf::Vector2f middle, int frame = 0)
    {
        const PropInfo* info = props::find(name);
        if (!info) return;
        sf::Sprite sprite(props::texture(), {info->rect.left + frame * info->rect.width, info->rect.top, info->rect.width, info->rect.height});
        sprite.setOrigin(std::floor(info->rect.width * 0.5f), std::floor(info->rect.height * 0.5f));
        sprite.setPosition(std::floor(middle.x), std::floor(middle.y));
        target.draw(sprite);
    }

    // The same, standing with its bottom (inside the outline) at `feet`, screen pixels.
    void drawStanding(sf::RenderTarget& target, const std::string& name, sf::Vector2f feet, float time = 0.f)
    {
        const PropInfo* info = props::find(name);
        if (!info) return;
        const int frame = info->frames > 1 ? static_cast<int>(time * info->fps) % info->frames : 0;
        sf::Sprite sprite(props::texture(), {info->rect.left + frame * info->rect.width, info->rect.top, info->rect.width, info->rect.height});
        sprite.setOrigin(std::floor(info->rect.width * 0.5f), static_cast<float>(info->rect.height) - 1.f);
        sprite.setPosition(std::floor(feet.x), std::floor(feet.y));
        target.draw(sprite);
    }
}

void Level::addIcicle(sf::Vector2i tile)
{
    const sf::Vector2f tip(tile.x * TILE_SIZE + TILE_SIZE * 0.5f, tile.y * TILE_SIZE);
    m_icicles.push_back({tip, tip});
}

void Level::addUpdraft(sf::Vector2i tile)
{
    int top = tile.y;
    while (top < tile.y + 60 && !isSolid(tile.x, top + 1) && !isSolid(tile.x + 1, top + 1)) ++top;
    m_updrafts.push_back({tile.x * TILE_SIZE, tile.y * TILE_SIZE, 2.f * TILE_SIZE, (top + 1 - tile.y) * TILE_SIZE});
}

void Level::addRing(sf::Vector2i tile, sf::Vector2i to, float period)
{
    const sf::Vector2f half(TILE_SIZE * 0.5f, TILE_SIZE * 0.5f);
    m_rings.push_back({sf::Vector2f(tile) * TILE_SIZE + half, sf::Vector2f(to) * TILE_SIZE + half, std::max(period, 0.5f)});
}

void Level::addLamp(sf::Vector2i tile)
{
    m_lamps.push_back({sf::Vector2f(tile) * TILE_SIZE});
}

void Level::addCart(sf::Vector2i tile, int dir, const std::string& needs)
{
    m_carts.push_back({sf::Vector2f(tile) * TILE_SIZE, dir < 0 ? -1 : 1, needs});
}

void Level::addCartStop(sf::Vector2i tile)
{
    m_cartStops.push_back(sf::Vector2f(tile) * TILE_SIZE);
}

void Level::addTrial(const std::string& route, bool finish, sf::Vector2i tile)
{
    m_trials.push_back({route, finish, sf::Vector2f(tile) * TILE_SIZE});
}

void Level::setWind(float wind) { m_wind = wind; }
void Level::setDark(bool dark) { m_dark = dark; }
void Level::setSpace(bool space) { m_space = space; }
float Level::wind() const { return m_wind; }
bool Level::isDark() const { return m_dark; }
bool Level::isSpace() const { return m_space; }
const std::vector<Cart>& Level::carts() const { return m_carts; }
const std::vector<sf::Vector2f>& Level::cartStops() const { return m_cartStops; }
const std::vector<Trial>& Level::trials() const { return m_trials; }

namespace
{
    // What a cart, a stop or a trial's post takes up: its tile and the one above.
    bool touches(sf::Vector2f pos, float height, float width, sf::Vector2f tile)
    {
        return pos.x < tile.x + Level::TILE_SIZE && pos.x + width > tile.x && pos.y < tile.y + 2.f * Level::TILE_SIZE && pos.y + height > tile.y;
    }
}

std::optional<size_t> Level::cartAt(sf::Vector2f pos, float height, float width) const
{
    for (size_t ii = 0; ii < m_carts.size(); ++ii) {
        if (touches(pos, height, width, m_carts[ii].position)) return ii;
    }
    return std::nullopt;
}

bool Level::isAtCartStop(sf::Vector2f pos, float height, float width) const
{
    return std::any_of(m_cartStops.begin(), m_cartStops.end(), [&](sf::Vector2f stop) { return touches(pos, height, width, stop); });
}

const Trial* Level::trialAt(sf::Vector2f pos, float height, float width) const
{
    for (const Trial& trial : m_trials) {
        if (touches(pos, height, width, trial.position)) return &trial;
    }
    return nullptr;
}

void Level::drawCarts(sf::RenderTarget& target, sf::Vector2f centr, const std::function<bool(const Cart&)>& ready) const
{
    for (const Cart& cart : m_carts) {
        drawStanding(target, ready(cart) ? "cart" : "cart_off", viewCoordinates(cart.position + sf::Vector2f(TILE_SIZE * 0.5f, 0.f), centr));
    }
    for (const sf::Vector2f& stop : m_cartStops) {
        drawStanding(target, "buffer", viewCoordinates(stop + sf::Vector2f(TILE_SIZE * 0.5f, 0.f), centr));
    }
}

void Level::drawRiddenCart(sf::RenderTarget& target, sf::Vector2f centr, sf::Vector2f bottom, float time) const
{
    drawStanding(target, "cart_rolling", viewCoordinates(bottom, centr), time);
}

void Level::drawTrials(sf::RenderTarget& target, sf::Vector2f centr, float time) const
{
    for (const Trial& trial : m_trials) {
        drawStanding(target, trial.finish ? "trial_finish" : "trial_start", viewCoordinates(trial.position + sf::Vector2f(TILE_SIZE * 0.5f, 0.f), centr), time);
    }
}
float Level::time() const { return m_time; }
const std::vector<Ring>& Level::rings() const { return m_rings; }
const std::vector<Lamp>& Level::lamps() const { return m_lamps; }
const std::vector<Icicle>& Level::icicles() const { return m_icicles; }
const std::vector<sf::FloatRect>& Level::updrafts() const { return m_updrafts; }

void Level::setLampLit(size_t lamp, bool lit)
{
    if (lamp < m_lamps.size()) m_lamps[lamp].lit = lit;
}

sf::Vector2f Level::ringPosition(size_t ring) const
{
    const Ring& r = m_rings[ring];
    // Easing in and out at the ends of its track, like on a pulley.
    const float along = 0.5f - 0.5f * std::cos(2.f * PI * m_time / r.period);
    return r.from + (r.to - r.from) * along;
}

Level::Crumbling* Level::crumbling(int x, int y)
{
    auto it = m_crumbling.find(tileKey(x, y));
    return it == m_crumbling.end() ? nullptr : &it->second;
}

const Level::Crumbling* Level::crumbling(int x, int y) const
{
    auto it = m_crumbling.find(tileKey(x, y));
    return it == m_crumbling.end() ? nullptr : &it->second;
}

float Level::updraftAt(sf::Vector2f point) const
{
    for (const sf::FloatRect& area : m_updrafts) {
        if (inside(area, point)) return 1.f;
    }
    return 0.f;
}

bool Level::isIce(int x, int y) const
{
    return tileAt(x, y) == TILE_ICE;
}

bool Level::isWater(int x, int y) const
{
    return tileAt(x, y) == TILE_WATER;
}

std::optional<float> Level::depthInWater(sf::Vector2f point) const
{
    auto [x, y] = tailNumbers(point);
    if (!isWater(x, y)) return std::nullopt;
    while (isWater(x, y + 1)) ++y;
    return (y + 1) * TILE_SIZE - point.y;
}

void Level::drawWater(sf::RenderTarget& target, sf::Vector2f centr, float time) const
{
    const sf::Vector2f halfScreen(config::GAME_WIDTH * 0.5f, config::GAME_HEIGHT * 0.5f);
    auto [x0, y0] = tailNumbers(centr - halfScreen);
    auto [x1, y1] = tailNumbers(centr + halfScreen);
    sf::VertexArray quads(sf::Quads);
    auto rect = [&](float left, float top, float width, float height, sf::Color colour) {
        quads.append({{left, top}, colour});
        quads.append({{left + width, top}, colour});
        quads.append({{left + width, top + height}, colour});
        quads.append({{left, top + height}, colour});
    };
    for (int x = x0 - 1; x <= x1 + 1; ++x) {
        for (int y = y0 - 1; y <= y1 + 1; ++y) {
            if (!isWater(x, y)) continue;
            // Deeper is darker; a band of light along the top, and the surface rippling.
            int depth = 0;
            while (depth < 6 && isWater(x, y + depth + 1)) ++depth;
            const sf::Vector2f at = viewCoordinates(sf::Vector2f(x * TILE_SIZE, (y + 1) * TILE_SIZE), centr);
            const float left = std::floor(at.x), top = std::floor(at.y);
            rect(left, top, TILE_SIZE, TILE_SIZE, sf::Color(30, 110 - 8 * depth, 150 - 6 * depth, static_cast<sf::Uint8>(120 + 14 * depth)));
            if (depth > 0) continue;
            for (int px = 0; px < static_cast<int>(TILE_SIZE); ++px) {
                const float wx = x * TILE_SIZE + static_cast<float>(px);
                const float wave = std::sin(wx * 0.35f + time * 2.2f) + std::sin(wx * 0.13f - time * 1.3f);
                const float lift = std::round(wave * 0.5f); // -1, 0 or 1 px
                rect(left + static_cast<float>(px), top + lift, 1.f, 1.f, sf::Color(170, 245, 255, static_cast<sf::Uint8>(110 + 35 * std::sin(wx * 0.7f + time * 3.f))));
                rect(left + static_cast<float>(px), top + lift + 1.f, 1.f, 2.f - lift, sf::Color(90, 210, 230, 45));
            }
            // Now and then a glint drifting along under the surface.
            const unsigned h = static_cast<unsigned>(x) * 2654435761u;
            const float glint = std::fmod(time * 0.8f + static_cast<float>(h % 1000) * 0.01f, 5.f);
            if (glint < 1.f) {
                const float gx = static_cast<float>((h >> 8) % 7);
                rect(left + gx, top + 3.f + static_cast<float>((h >> 4) % 3), 1.f, 1.f, sf::Color(220, 255, 255, static_cast<sf::Uint8>(200.f * (1.f - glint))));
            }
        }
    }
    target.draw(quads);
}

bool Level::isOnIce(sf::Vector2f feet, float width) const
{
    const float row = feet.y / TILE_SIZE;
    if (row != std::floor(row)) return false;
    const int y = static_cast<int>(row) - 1;
    for (int x = static_cast<int>(std::floor(feet.x / TILE_SIZE)); x * TILE_SIZE < feet.x + width; ++x) {
        if (isIce(x, y)) return true;
    }
    return false;
}

void Level::update(float dt, sf::FloatRect player)
{
    m_time += dt;

    // In a hidden passage, it fades away round you; out of it, it closes up again.
    bool inPassage = false;
    for (int x = static_cast<int>(std::floor(player.left / TILE_SIZE)); x * TILE_SIZE < player.left + player.width && !inPassage; ++x) {
        for (int y = static_cast<int>(std::floor(player.top / TILE_SIZE)); y * TILE_SIZE < player.top + player.height; ++y) {
            if (isHidden(x, y)) inPassage = true;
        }
    }
    m_reveal = inPassage ? std::min(1.f, m_reveal + dt * 4.f) : std::max(0.f, m_reveal - dt * 2.f);

    // Crumbling blocks and clouds: stood on, they start to give; once started, they go.
    const float feetRow = player.top / TILE_SIZE;
    if (feetRow == std::floor(feetRow)) {
        const int y = static_cast<int>(feetRow) - 1;
        for (int x = static_cast<int>(std::floor(player.left / TILE_SIZE)); x * TILE_SIZE < player.left + player.width; ++x) {
            const int tile = tileAt(x, y);
            if (tile != TILE_CRUMBLE && tile != TILE_CLOUD) continue;
            Crumbling& state = m_crumbling[tileKey(x, y)];
            if (state.gone <= 0.f && state.stood <= 0.f) {
                state.stood = 0.001f;
                const sf::Vector2f middle((x + 0.5f) * TILE_SIZE, (y + 0.5f) * TILE_SIZE);
                m_sounds.push_back({tile == TILE_CLOUD ? LevelSound::Kind::CloudFade : LevelSound::Kind::Crack, middle});
            }
        }
    }
    for (auto& [key, state] : m_crumbling) {
        const int x = static_cast<int>(key >> 32), y = static_cast<int>(static_cast<unsigned>(key & 0xffffffff));
        const bool cloud = tileAt(x, y) == TILE_CLOUD;
        if (state.gone > 0.f) {
            state.gone -= dt;
            // Not back while you're in its way.
            const sf::FloatRect tileBox(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE);
            if (state.gone <= 0.f && tileBox.intersects(player)) state.gone = 0.1f;
        } else if (state.stood > 0.f) {
            state.stood += dt;
            if (state.stood >= (cloud ? CLOUD_DELAY : CRUMBLE_DELAY)) {
                state.stood = 0.f;
                state.gone = cloud ? CLOUD_BACK : CRUMBLE_BACK;
                if (!cloud) m_sounds.push_back({LevelSound::Kind::Crumble, {(x + 0.5f) * TILE_SIZE, (y + 0.5f) * TILE_SIZE}});
            }
        }
    }

    // Icicles: notice you below, shake, fall, shatter, grow back.
    for (Icicle& icicle : m_icicles) {
        switch (icicle.state) {
            case Icicle::State::Hanging: {
                const bool under = player.left < icicle.tip.x + 6.f && player.left + player.width > icicle.tip.x - 6.f &&
                                   player.top + player.height < icicle.tip.y && player.top > icicle.tip.y - ICICLE_SIGHT * TILE_SIZE;
                bool clear = under;
                for (float y = icicle.tip.y - 1.f; clear && y > player.top + player.height; y -= TILE_SIZE) {
                    const auto [tx, ty] = tailNumbers({icicle.tip.x, y});
                    clear = !isSolid(tx, ty);
                }
                if (clear) {
                    icicle.state = Icicle::State::Shaking;
                    icicle.timer = ICICLE_SHAKE;
                    m_sounds.push_back({LevelSound::Kind::IcicleShake, icicle.tip});
                }
                break;
            }
            case Icicle::State::Shaking:
                icicle.timer -= dt;
                if (icicle.timer <= 0.f) {
                    icicle.state = Icicle::State::Falling;
                    icicle.speed = 0.f;
                }
                break;
            case Icicle::State::Falling: {
                icicle.speed = std::min(icicle.speed + ICICLE_GRAVITY * dt, ICICLE_MAX_SPEED);
                icicle.tip.y -= icicle.speed * dt;
                const auto [tx, ty] = tailNumbers(icicle.tip);
                if (isSolid(tx, ty) || icicle.tip.y < m_bounds.top - 2.f * TILE_SIZE) {
                    icicle.state = Icicle::State::Gone;
                    icicle.timer = ICICLE_BACK;
                    m_sounds.push_back({LevelSound::Kind::IcicleShatter, icicle.tip});
                }
                break;
            }
            case Icicle::State::Gone:
                icicle.timer -= dt;
                if (icicle.timer <= 0.f) {
                    icicle.state = Icicle::State::Hanging;
                    icicle.tip = icicle.home;
                }
                break;
        }
    }
}

std::vector<LevelSound> Level::takeSounds()
{
    return std::exchange(m_sounds, {});
}

void Level::reset()
{
    m_time = 0.f;
    m_reveal = 0.f;
    m_crumbling.clear();
    m_sounds.clear();
    for (Icicle& icicle : m_icicles) {
        icicle.state = Icicle::State::Hanging;
        icicle.tip = icicle.home;
        icicle.speed = icicle.timer = 0.f;
    }
}

std::optional<RopeHit> Level::ropeHit(sf::Vector2f from, sf::Vector2f to, float range) const
{
    const float length = distance(from, to);
    if (length < 0.001f) return std::nullopt;
    const sf::Vector2f dir = (to - from) / length;
    constexpr float STEP = 0.25f;
    sf::Vector2f lastEmpty = from;
    std::vector<sf::Vector2f> rings;
    for (size_t ii = 0; ii < m_rings.size(); ++ii) rings.push_back(ringPosition(ii));
    for (float d = 0.f; d <= range; d += STEP) {
        const sf::Vector2f p = from + dir * d;
        for (size_t ii = 0; ii < rings.size(); ++ii) {
            if (distance(p, rings[ii]) <= RING_RADIUS) return RopeHit{RopeHit::Kind::Ring, rings[ii], static_cast<int>(ii), d};
        }
        for (size_t ii = 0; ii < m_levers.size(); ++ii) {
            if (inside({m_levers[ii].x, m_levers[ii].y, TILE_SIZE, TILE_SIZE}, p)) return RopeHit{RopeHit::Kind::Lever, p, static_cast<int>(ii), d};
        }
        for (size_t ii = 0; ii < m_lamps.size(); ++ii) {
            const sf::Vector2f at = m_lamps[ii].position;
            if (inside({at.x + 1.f, at.y, TILE_SIZE - 2.f, TILE_SIZE}, p)) return RopeHit{RopeHit::Kind::Lamp, p, static_cast<int>(ii), d};
        }
        const auto [tx, ty] = tailNumbers(p);
        // Slick stone and soft mushroom caps: nothing to bite.
        if (isSolid(tx, ty)) return RopeHit{isSlick(tx, ty) || isBounce(tx, ty) ? RopeHit::Kind::Slick : RopeHit::Kind::Rock, lastEmpty, -1, d};
        lastEmpty = p;
    }
    return std::nullopt;
}

void Level::drawMoving(sf::RenderTarget& target, sf::Vector2f centr, float time) const
{
    // Updrafts: heat rising, in streaks.
    for (const sf::FloatRect& area : m_updrafts) {
        // A faint haze over the whole column, so you can see where it is.
        const sf::Vector2f corner = viewCoordinates({area.left, area.top + area.height}, centr);
        sf::RectangleShape haze({area.width, area.height});
        haze.setPosition(std::floor(corner.x), std::floor(corner.y));
        haze.setFillColor(sf::Color(255, 150, 80, 18));
        target.draw(haze);
        const int streaks = std::max(6, static_cast<int>(area.height / 10.f));
        for (int i = 0; i < streaks; ++i) {
            const float x = area.left + 2.f + std::fmod(i * 5.3f, area.width - 4.f);
            const float rise = std::fmod(time * (50.f + (i % 5) * 9.f) + i * 37.f, area.height);
            const sf::Vector2f p = viewCoordinates({x, area.top + rise}, centr);
            const auto alpha = static_cast<sf::Uint8>(150.f * (1.f - rise / area.height));
            sf::RectangleShape streak({1.f, 5.f});
            streak.setPosition(std::floor(p.x), std::floor(p.y));
            streak.setFillColor(sf::Color(255, 190, 120, alpha));
            target.draw(streak);
        }
        drawProp(target, "vent", viewCoordinates({area.left + TILE_SIZE, area.top + 2.f}, centr));
    }
    // Rings on their tracks.
    for (size_t ii = 0; ii < m_rings.size(); ++ii) {
        const sf::Vector2f a = viewCoordinates(m_rings[ii].from, centr), b = viewCoordinates(m_rings[ii].to, centr);
        const sf::Color track(70, 70, 90);
        const sf::Vertex line[] = {{{std::floor(a.x) + 0.5f, std::floor(a.y) + 0.5f}, track}, {{std::floor(b.x) + 0.5f, std::floor(b.y) + 0.5f}, track}};
        target.draw(line, 2, sf::Lines);
        drawProp(target, "ring", viewCoordinates(ringPosition(ii), centr));
    }
    // Lamps, lit or not.
    for (const Lamp& lamp : m_lamps) {
        drawProp(target, lamp.lit ? "lamp_lit" : "lamp", viewCoordinates(lamp.position + sf::Vector2f(TILE_SIZE * 0.5f, TILE_SIZE * 0.5f), centr));
    }
    // Icicles, shaking before they fall.
    for (const Icicle& icicle : m_icicles) {
        if (icicle.state == Icicle::State::Gone) continue;
        const float shake = icicle.state == Icicle::State::Shaking ? std::round(std::sin(time * 70.f)) : 0.f;
        drawProp(target, "icicle", viewCoordinates(icicle.tip + sf::Vector2f(shake, 4.f), centr));
    }
}
