#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <SFML/Graphics.hpp>
#include "Level.h"

struct RopeNode
{
    sf::Vector2f position;
    sf::Vector2f previousPosition;
    bool isFixed;

    RopeNode(sf::Vector2f pos, bool isF) : position(pos), previousPosition(pos), isFixed(isF) {}
};

// A grappling rope between the player and an anchor point on a wall.
//
// For gameplay the rope is a maximum distance: the player moves freely inside a circle around the
// rope's last pivot, and the rope only pulls when it's taut. When the rope swings into a corner of
// the level it wraps around it, and that corner becomes the new pivot, with whatever length is left.
// Swinging back the other way unwraps it again.
//
// The chain of nodes between the last pivot and the player is cosmetic: it sags when the rope is
// slack and rests on the level instead of hanging through it.
class Rope
{
public:
    static constexpr float MIN_LENGTH = 8.f;
    static constexpr float MAX_LENGTH = 120.f;
    static constexpr int NUM_SEGMENTS = 24;

    Rope(sf::Vector2f attachPos, sf::Vector2f anchor, std::shared_ptr<const Level> level);

    // Where the rope is stuck in the wall.
    sf::Vector2f anchor() const;
    // Moves where it's stuck: on a ring sliding along its track, or in a stone being dragged. The
    // part between the last corner and the player stays as long as it was. Where the anchor has come
    // round (or up to) the first corner the rope was wrapped on, it isn't wrapped on it any more.
    void moveAnchor(sf::Vector2f anchor, sf::Vector2f attachPos);
    // The point the player swings around: the anchor, or the last corner the rope wrapped around.
    sf::Vector2f pivot() const;
    // The anchor followed by every corner the rope is wrapped around, in order.
    const std::vector<sf::Vector2f>& pivots() const;
    // Total length, including the parts wrapped around corners.
    float length() const;
    // Length left between the last pivot and the player.
    float freeLength() const;
    // Clamped to [MIN_LENGTH, MAX_LENGTH], and never shorter than the wrapped part plus MIN_LENGTH.
    void setLength(float length);

    bool isTaut(sf::Vector2f attachPos) const;
    // Removes the part of `velocity` that would stretch the rope.
    sf::Vector2f constrainVelocity(sf::Vector2f attachPos, sf::Vector2f velocity) const;
    // The closest point to `attachPos` that the rope allows.
    sf::Vector2f constrainPosition(sf::Vector2f attachPos) const;

    // Wraps the rope around corners it swept across while the player moved from `from` to `to`,
    // and unwraps corners it has swung back off.
    void wrap(sf::Vector2f from, sf::Vector2f to);

    // Simulates the cosmetic chain with its first node pinned to `attachPos`.
    void update(sf::Vector2f attachPos, float dt);
    void draw(sf::RenderTarget& target, sf::Vector2f centr) const;
    const std::vector<RopeNode>& getNodes() const;
    // Where the rope lies, from the anchor round the corners and down the chain to the player.
    std::vector<sf::Vector2f> path() const;

private:
    bool isBlocked(sf::Vector2f from, sf::Vector2f to) const;
    std::optional<sf::Vector2f> firstCornerSwept(sf::Vector2f pivot, sf::Vector2f from, sf::Vector2f to) const;
    void unwrap(sf::Vector2f attachPos);
    void resetChain(sf::Vector2f attachPos);
    void pushChainOutOfWalls();

    std::shared_ptr<const Level> m_level;
    std::vector<RopeNode> nodes;
    std::vector<sf::Vector2f> m_pivots;  // m_pivots[0] is the anchor
    std::vector<float> m_sides;          // which way the rope bends around m_pivots[i + 1]; it unwraps when this flips
    float m_length;
    float m_wrappedLength = 0.f;         // along the pivots, from the anchor to the last one
};
