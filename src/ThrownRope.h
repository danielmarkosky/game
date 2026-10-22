#pragma once

#include "src/Level.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>

// The rope when it isn't hooked: paid out behind the hook as it flies, and reeled back in after a
// miss or after letting go. A chain of points a few pixels apart that falls, rests on the level and
// slides along it, with your hand holding one end. Reeling in pulls the rope in at the hand, so it
// slides back the way it lies, around corners and along the ground, and it takes as long as the
// rope is long, not as the crow flies.
class ThrownRope
{
public:
    static constexpr float SEGMENT = 4.f; // between the points

    // Thrown from `hand`: nothing paid out yet, the hook still in the hand.
    ThrownRope(sf::Vector2f hand, std::shared_ptr<const Level> level);
    // Let go of where it lies: `path` from the hook to the hand (see Rope::path()).
    ThrownRope(const std::vector<sf::Vector2f>& path, std::shared_ptr<const Level> level);

    // Flying out: the hook is at `head`, paying out rope behind it.
    void flyTo(sf::Vector2f head);
    // The hook hit something it doesn't stick to: it bounces back off it a little, then it's loose.
    void bounce(sf::Vector2f away);
    // Pulled in at the hand, by `amount` of rope.
    void reelIn(float amount);
    // Moves the rope on by `dt`, with its end in `hand`. While flying the hook stays where flyTo put it.
    void update(sf::Vector2f hand, float dt, bool flying);

    float length() const; // out of the hand
    bool isIn() const;    // all reeled in
    sf::Vector2f hook() const;
    const std::vector<sf::Vector2f>& points() const; // from the hook to the hand

    void draw(sf::RenderTarget& target, sf::Vector2f centr) const;

private:
    void addPointAtHand();
    // As far from `from` towards `to` as it gets without going into rock.
    sf::Vector2f sweep(sf::Vector2f from, sf::Vector2f to) const;

    std::shared_ptr<const Level> level;
    std::vector<sf::Vector2f> position; // [0] the hook, back() the hand
    std::vector<sf::Vector2f> previous;
    float handSegment = 0.f; // the length of rope between the hand and the point next to it
};
