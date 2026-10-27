#include "ThrownRope.h"
#include "src/Physics.h"
#include "src/utils.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr int ITERATIONS = 16;
    constexpr float DAMPING = 0.99f;  // of each point's speed, every step
    constexpr float FRICTION = 0.6f;  // how much sideways speed a point keeps touching the ground
    constexpr float BOUNCE = 1.5f;    // how fast the hook flies back off what it bounced off, px a step
    constexpr float MAX_STEP = 4.f;   // the fastest a point moves on its own, px a step

    const sf::Color ROPE_COLOUR(200, 190, 170);
    const sf::Color HOOK_COLOUR(255, 90, 90);
}

ThrownRope::ThrownRope(sf::Vector2f hand, std::shared_ptr<const Level> lvl)
    : level(std::move(lvl)), position{hand, hand}, previous{hand, hand}
{
}

ThrownRope::ThrownRope(const std::vector<sf::Vector2f>& path, std::shared_ptr<const Level> lvl)
    : level(std::move(lvl))
{
    if (path.empty()) return;
    // A point every SEGMENT along the path, from the hook; what's left over is the hand's segment.
    position.push_back(path.front());
    float carried = 0.f; // along the path since the last point
    for (size_t ii = 1; ii < path.size(); ++ii) {
        sf::Vector2f from = path[ii - 1];
        const sf::Vector2f to = path[ii];
        float left = distance(from, to);
        while (carried + left >= SEGMENT) {
            const float step = SEGMENT - carried;
            from += (to - from) * (step / left);
            left -= step;
            carried = 0.f;
            position.push_back(from);
        }
        carried += left;
    }
    handSegment = carried;
    position.push_back(path.back());
    previous = position;
}

void ThrownRope::flyTo(sf::Vector2f head)
{
    position.front() = previous.front() = head;
}

void ThrownRope::bounce(sf::Vector2f away)
{
    const float length = magnitude(away);
    if (length > 0.f) previous.front() = position.front() - away / length * BOUNCE;
}

void ThrownRope::reelIn(float amount)
{
    handSegment -= amount;
    // The point next to the hand has come in: the one before it is next.
    while (handSegment <= 0.f && position.size() > 2) {
        position.erase(position.end() - 2);
        previous.erase(previous.end() - 2);
        handSegment += SEGMENT;
    }
    handSegment = std::max(handSegment, 0.f);
}

void ThrownRope::addPointAtHand()
{
    const sf::Vector2f last = position[position.size() - 2], hand = position.back();
    const float apart = distance(last, hand);
    const sf::Vector2f point = apart > 0.f ? last + (hand - last) * (SEGMENT / apart) : hand;
    position.insert(position.end() - 1, point);
    previous.insert(previous.end() - 1, point);
    handSegment -= SEGMENT;
}

void ThrownRope::update(sf::Vector2f hand, float dt, bool flying)
{
    if (position.size() < 2) return;
    position.back() = previous.back() = hand;
    if (flying) {
        // Paid out as the hook pulls it: as much as it takes to reach.
        handSegment += std::max(0.f, distance(hand, position.front()) - length());
        while (handSegment > SEGMENT) addPointAtHand();
    }

    const size_t last = position.size() - 1;
    const size_t first = flying ? 1 : 0; // a flying hook goes where it's thrown
    const float fall = level->isSpace() ? 0.f : physics().gravity * dt * dt; // nothing falls in the Still
    const std::vector<sf::Vector2f> start = position;
    for (size_t ii = first; ii < last; ++ii) {
        sf::Vector2f speed = (position[ii] - previous[ii]) * DAMPING;
        if (const float fast = magnitude(speed); fast > MAX_STEP) speed *= MAX_STEP / fast;
        previous[ii] = position[ii];
        position[ii] += speed;
        position[ii].y -= fall;
    }

    for (int it = 0; it < ITERATIONS; ++it) {
        // A rope can go slack, but never stretch.
        for (size_t ii = 0; ii < last; ++ii) {
            const float rest = ii + 1 == last ? handSegment : SEGMENT;
            const sf::Vector2f delta = position[ii + 1] - position[ii];
            const float length = magnitude(delta);
            if (length <= rest || length < 0.0001f) continue;
            const float wA = ii < first ? 0.f : 1.f, wB = ii + 1 == last ? 0.f : 1.f;
            if (wA + wB == 0.f) continue;
            const sf::Vector2f correction = delta * ((length - rest) / length / (wA + wB));
            position[ii] += correction * wA;
            position[ii + 1] -= correction * wB;
        }
        for (size_t ii = first; ii < last; ++ii) {
            if (!pushOutOfRock(*level, position[ii])) continue;
            // Lying on it: no bouncing off, and dragging along it slows down.
            previous[ii].x = position[ii].x - (position[ii].x - previous[ii].x) * FRICTION;
            previous[ii].y = position[ii].y;
        }
    }

    // Never through rock: each point gets from where it was to where it's going only as far as it
    // can without passing through any. (Pushing out of rock alone could pop a point out of the far
    // side of a thin wall or floor.)
    for (size_t ii = first; ii < last; ++ii) {
        const sf::Vector2f to = position[ii];
        position[ii] = sweep(start[ii], to);
        if (position[ii] != to) previous[ii] = position[ii];
    }
}

sf::Vector2f ThrownRope::sweep(sf::Vector2f from, sf::Vector2f to) const
{
    const auto inRock = [&](sf::Vector2f p) {
        const auto [x, y] = tailNumbers(p);
        return level->isSolid(x, y);
    };
    if (inRock(from)) return to;
    const float length = distance(from, to);
    const int steps = static_cast<int>(std::ceil(length / 0.5f));
    sf::Vector2f free = from;
    for (int step = 1; step <= steps; ++step) {
        const sf::Vector2f p = from + (to - from) * (static_cast<float>(step) / static_cast<float>(steps));
        if (inRock(p)) return free;
        free = p;
    }
    return to;
}

float ThrownRope::length() const
{
    return SEGMENT * static_cast<float>(position.size() > 2 ? position.size() - 2 : 0) + handSegment;
}

bool ThrownRope::isIn() const
{
    return position.size() <= 2 && handSegment <= 0.f;
}

sf::Vector2f ThrownRope::hook() const
{
    return position.empty() ? sf::Vector2f() : position.front();
}

const std::vector<sf::Vector2f>& ThrownRope::points() const
{
    return position;
}

void ThrownRope::draw(sf::RenderTarget& target, sf::Vector2f centr) const
{
    if (position.size() < 2) return;
    sf::VertexArray line(sf::LineStrip);
    for (const sf::Vector2f& point : position) {
        const sf::Vector2f p = viewCoordinates(point, centr);
        line.append(sf::Vertex({std::floor(p.x) + 0.5f, std::floor(p.y) + 0.5f}, ROPE_COLOUR));
    }
    target.draw(line);
    const sf::Vector2f hook = viewCoordinates(position.front(), centr);
    sf::RectangleShape shape({2.f, 2.f});
    shape.setPosition(std::floor(hook.x) - 1.f, std::floor(hook.y) - 1.f);
    shape.setFillColor(HOOK_COLOUR);
    target.draw(shape);
}
