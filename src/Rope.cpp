#include "Rope.h"
#include "src/Physics.h"
#include "Config.h"
#include "utils.h"
#include <algorithm>
#include <cmath>

namespace
{
    // Pivots sit this far off the corner they wrap, on the open side, so the rope never
    // starts inside rock.
    constexpr float WRAP_OFFSET = 0.3f;
    // More wraps than this in a single step means something degenerate; stop instead of looping.
    constexpr int MAX_WRAPS_PER_STEP = 8;

    float cross(sf::Vector2f a, sf::Vector2f b)
    {
        return a.x * b.y - a.y * b.x;
    }

    // Signed distance of `p` from the line through `a` and `b` (positive on the left).
    float sideOfLine(sf::Vector2f a, sf::Vector2f b, sf::Vector2f p)
    {
        float len = magnitude(b - a);
        if (len < 0.001f) return 0.f;
        return cross(b - a, p - a) / len;
    }

    bool inTriangle(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Vector2f p)
    {
        const float tolerance = 0.25f;
        float d1 = sideOfLine(a, b, p), d2 = sideOfLine(b, c, p), d3 = sideOfLine(c, a, p);
        bool hasNeg = d1 < -tolerance || d2 < -tolerance || d3 < -tolerance;
        bool hasPos = d1 > tolerance || d2 > tolerance || d3 > tolerance;
        return !(hasNeg && hasPos);
    }
}

Rope::Rope(sf::Vector2f attachPos, sf::Vector2f anchor, std::shared_ptr<const Level> level)
    : m_level(std::move(level)), m_length(std::clamp(distance(attachPos, anchor), MIN_LENGTH, MAX_LENGTH))
{
    m_pivots.push_back(anchor);
    resetChain(attachPos);
}

sf::Vector2f Rope::anchor() const { return m_pivots.front(); }

void Rope::moveAnchor(sf::Vector2f anchor, sf::Vector2f attachPos)
{
    const auto measureWrapped = [this] {
        m_wrappedLength = 0.f;
        for (size_t ii = 1; ii < m_pivots.size(); ++ii) m_wrappedLength += distance(m_pivots[ii - 1], m_pivots[ii]);
    };
    // The part from the last corner to the player stays as it was...
    const float free = freeLength();
    m_pivots.front() = anchor;
    measureWrapped();
    m_length = std::clamp(m_wrappedLength + free, MIN_LENGTH, MAX_LENGTH);
    // ...and where it straightens out past the first corner, it's the same rope, just not bent.
    const size_t before = m_pivots.size();
    while (m_pivots.size() > 1) {
        const sf::Vector2f corner = m_pivots[1];
        const sf::Vector2f next = m_pivots.size() > 2 ? m_pivots[2] : attachPos;
        // Still bent round it the same way, or with rock in the way straight past it, and not up
        // against it: keep it.
        const bool bent = cross(corner - anchor, next - corner) * m_sides.front() >= 0.f;
        if (distance(anchor, corner) > 2.f && (bent || isBlocked(anchor, next))) break;
        m_pivots.erase(m_pivots.begin() + 1);
        m_sides.erase(m_sides.begin());
    }
    if (m_pivots.size() != before) {
        measureWrapped();
        resetChain(attachPos);
    }
}

sf::Vector2f Rope::pivot() const { return m_pivots.back(); }

const std::vector<sf::Vector2f>& Rope::pivots() const { return m_pivots; }

float Rope::length() const { return m_length; }

float Rope::freeLength() const { return std::max(0.f, m_length - m_wrappedLength); }

void Rope::setLength(float length)
{
    float shortest = std::min(MAX_LENGTH, std::max(MIN_LENGTH, m_wrappedLength + MIN_LENGTH));
    m_length = std::clamp(length, shortest, MAX_LENGTH);
}

bool Rope::isTaut(sf::Vector2f attachPos) const
{
    return distance(attachPos, pivot()) >= freeLength() - 0.01f;
}

sf::Vector2f Rope::constrainVelocity(sf::Vector2f attachPos, sf::Vector2f velocity) const
{
    sf::Vector2f fromPivot = attachPos - pivot();
    float dist = magnitude(fromPivot);
    if (dist < freeLength() - 0.01f || dist < 0.001f) return velocity;

    // Only the outward part is removed, so the player keeps swinging along the circle.
    sf::Vector2f dir = fromPivot / dist;
    float outward = dot(velocity, dir);
    if (outward > 0.f) velocity -= outward * dir;
    return velocity;
}

sf::Vector2f Rope::constrainPosition(sf::Vector2f attachPos) const
{
    sf::Vector2f fromPivot = attachPos - pivot();
    float dist = magnitude(fromPivot);
    if (dist <= freeLength() || dist < 0.001f) return attachPos;
    return pivot() + fromPivot * (freeLength() / dist);
}

void Rope::wrap(sf::Vector2f from, sf::Vector2f to)
{
    const size_t before = m_pivots.size();
    unwrap(to);
    bool changed = m_pivots.size() != before;

    for (int ii = 0; ii < MAX_WRAPS_PER_STEP && isBlocked(pivot(), to); ++ii) {
        auto corner = firstCornerSwept(pivot(), from, to);
        if (!corner) break;
        float side = cross(*corner - pivot(), to - *corner);
        m_wrappedLength += distance(pivot(), *corner);
        m_pivots.push_back(*corner);
        m_sides.push_back(side < 0.f ? -1.f : 1.f);
        changed = true;
    }

    if (changed) resetChain(to);
}

void Rope::unwrap(sf::Vector2f attachPos)
{
    while (m_pivots.size() > 1) {
        sf::Vector2f last = m_pivots.back();
        sf::Vector2f previous = m_pivots[m_pivots.size() - 2];
        // Still bent around the corner the same way it wrapped: keep it.
        if (cross(last - previous, attachPos - last) * m_sides.back() >= 0.f) break;
        m_wrappedLength -= distance(previous, last);
        m_pivots.pop_back();
        m_sides.pop_back();
    }
    if (m_pivots.size() == 1) m_wrappedLength = 0.f;
}

bool Rope::isBlocked(sf::Vector2f from, sf::Vector2f to) const
{
    const float step = 0.5f;
    int steps = static_cast<int>(std::ceil(distance(from, to) / step));
    // The ends are the pivot and the player, both outside rock; only the part between matters.
    for (int ii = 1; ii < steps; ++ii) {
        auto [tx, ty] = tailNumbers(from + (to - from) * (static_cast<float>(ii) / steps));
        if (m_level->isSolid(tx, ty)) return true;
    }
    return false;
}

std::optional<sf::Vector2f> Rope::firstCornerSwept(sf::Vector2f pivot, sf::Vector2f from, sf::Vector2f to) const
{
    // As the player moved from `from` to `to`, the rope swept the triangle (pivot, from, to). It
    // catches on whichever outside corner of the rock it reaches first while turning from `from`.
    const float T = Level::TILE_SIZE;
    const int x0 = static_cast<int>(std::floor(std::min({pivot.x, from.x, to.x}) / T));
    const int x1 = static_cast<int>(std::ceil(std::max({pivot.x, from.x, to.x}) / T));
    const int y0 = static_cast<int>(std::floor(std::min({pivot.y, from.y, to.y}) / T));
    const int y1 = static_cast<int>(std::ceil(std::max({pivot.y, from.y, to.y}) / T));

    const sf::Vector2f startDir = from - pivot;
    std::optional<sf::Vector2f> best;
    float bestAngle = 0.f, bestDist = 0.f;

    for (int cx = x0; cx <= x1; ++cx) {
        for (int cy = y0; cy <= y1; ++cy) {
            // The four tiles around this corner, and the direction from the corner into each.
            const bool solid[4] = {m_level->isSolid(cx - 1, cy - 1), m_level->isSolid(cx, cy - 1),
                                   m_level->isSolid(cx - 1, cy), m_level->isSolid(cx, cy)};
            const sf::Vector2f dirs[4] = {{-1.f, -1.f}, {1.f, -1.f}, {-1.f, 1.f}, {1.f, 1.f}};
            // An outside corner has exactly one rock tile; the rope slides along flat walls and
            // into inside corners without catching.
            if (std::count(solid, solid + 4, true) != 1) continue;

            const sf::Vector2f corner(cx * T, cy * T);
            const float dist = distance(corner, pivot);
            if (dist < 1.f || !inTriangle(pivot, from, to, corner)) continue;

            const sf::Vector2f dir = corner - pivot;
            const float angle = std::abs(std::atan2(cross(startDir, dir), dot(startDir, dir)));
            const bool better = !best || angle < bestAngle - 1e-4f || (angle < bestAngle + 1e-4f && dist < bestDist);
            if (!better) continue;

            sf::Vector2f away;
            for (int ii = 0; ii < 4; ++ii) {
                if (!solid[ii]) away += dirs[ii];
            }
            best = corner + away / magnitude(away) * WRAP_OFFSET;
            bestAngle = angle;
            bestDist = dist;
        }
    }
    return best;
}

void Rope::resetChain(sf::Vector2f attachPos)
{
    nodes.clear();
    nodes.reserve(NUM_SEGMENTS + 1);
    for (int ii = 0; ii <= NUM_SEGMENTS; ii++) {
        float t = static_cast<float>(ii) / static_cast<float>(NUM_SEGMENTS);
        nodes.emplace_back(attachPos + t * (pivot() - attachPos), ii == NUM_SEGMENTS);
    }
}

void Rope::update(sf::Vector2f attachPos, float dt)
{
    const int iterations = 20;
    const float damping = 0.98f;
    const float segmentLength = std::max(freeLength(), 0.1f) / NUM_SEGMENTS;

    nodes.front().position = attachPos;
    nodes.front().previousPosition = attachPos;
    nodes.back().position = pivot();
    nodes.back().previousPosition = pivot();

    // A taut rope is a straight line. Simulating it would make the chain lag behind fast swings.
    if (isTaut(attachPos)) {
        for (size_t ii = 1; ii + 1 < nodes.size(); ii++) {
            float t = static_cast<float>(ii) / static_cast<float>(NUM_SEGMENTS);
            nodes[ii].position = attachPos + t * (pivot() - attachPos);
            nodes[ii].previousPosition = nodes[ii].position;
        }
        return;
    }

    for (size_t ii = 1; ii < nodes.size(); ii++) {
        RopeNode& node = nodes[ii];
        if (node.isFixed) continue;
        sf::Vector2f velocity = node.position - node.previousPosition;
        node.previousPosition = node.position;
        node.position += velocity * damping;
        if (!m_level->isSpace()) node.position.y -= physics().gravity * dt * dt;
    }

    for (int it = 0; it < iterations; ++it) {
        for (size_t ii = 0; ii + 1 < nodes.size(); ii++) {
            RopeNode& a = nodes[ii];
            RopeNode& b = nodes[ii + 1];
            sf::Vector2f delta = b.position - a.position;
            float currentLength = magnitude(delta);
            if (currentLength <= segmentLength || currentLength < 0.001f) continue; // slack is fine

            // The player end is pinned too, so neither end of the chain moves.
            float wA = (a.isFixed || ii == 0) ? 0.f : 1.f;
            float wB = b.isFixed ? 0.f : 1.f;
            float total = wA + wB;
            if (total <= 0.f) continue;

            sf::Vector2f correction = delta * ((currentLength - segmentLength) / currentLength);
            a.position += correction * (wA / total);
            b.position -= correction * (wB / total);
        }
        pushChainOutOfWalls();
    }
}

void Rope::pushChainOutOfWalls()
{
    for (size_t ii = 1; ii + 1 < nodes.size(); ii++) {
        RopeNode& node = nodes[ii];
        // Resting on the surface: no sliding momentum.
        if (pushOutOfRock(*m_level, node.position)) node.previousPosition = node.position;
    }
}

void Rope::draw(sf::RenderTarget& target, sf::Vector2f centr) const
{
    const sf::Color ropeColor(200, 190, 170);
    auto toScreen = [&](sf::Vector2f world) {
        sf::Vector2f p = viewCoordinates(world, centr);
        return sf::Vertex({std::floor(p.x) + 0.5f, std::floor(p.y) + 0.5f}, ropeColor);
    };

    // Anchor -> wrapped corners -> down the chain to the player.
    sf::VertexArray line(sf::LineStrip);
    for (const auto& p : m_pivots) line.append(toScreen(p));
    for (size_t ii = nodes.size() - 1; ii-- > 0;) line.append(toScreen(nodes[ii].position));
    target.draw(line);

    sf::Vector2f hook = viewCoordinates(anchor(), centr);
    sf::RectangleShape hookShape({2.f, 2.f});
    hookShape.setPosition(std::floor(hook.x) - 1.f, std::floor(hook.y) - 1.f);
    hookShape.setFillColor(sf::Color(255, 90, 90));
    target.draw(hookShape);
}

const std::vector<RopeNode>& Rope::getNodes() const { return nodes; }

std::vector<sf::Vector2f> Rope::path() const
{
    std::vector<sf::Vector2f> points(m_pivots);
    for (size_t ii = nodes.size() - 1; ii-- > 0;) points.push_back(nodes[ii].position);
    return points;
}
