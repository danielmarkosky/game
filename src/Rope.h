#pragma once

#include <vector>
#include <memory>
#include <SFML/Graphics.hpp>
#include "Level.h"

struct RopeNode
{
    sf::Vector2f position;
    sf::Vector2f previousPosition;
    bool isFixed;

    RopeNode(sf::Vector2f pos, bool isF) : position(pos), previousPosition(pos), isFixed(isF) {}
};

class Rope
{
public:
    Rope(sf::Vector2f startPos, sf::Vector2f endPos, std::shared_ptr<Level> level);
    void update(float dt);
    void draw(sf::RenderTarget& target, sf::Vector2f centr) const;
    const std::vector<RopeNode>& getNodes() const;

    sf::Vector2f getNodePosition(int index) const;
    sf::Vector2f getNodeVelocity(int index, float dt) const;
    void setNodePosition(int index, sf::Vector2f pos);
    void setNodeVelocity(int index, sf::Vector2f velocity, float dt);
    void applyForceToNode(int index, sf::Vector2f force);
    void applyFrictionToNode(int index, float factor);

private:
    std::vector<RopeNode> nodes;
    std::shared_ptr<Level> m_level;
    float m_segmentLength;
};
