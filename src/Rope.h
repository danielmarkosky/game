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
    Rope(sf::Vector2f startPos, sf::Vector2f endPos, int numNodes, std::shared_ptr<Level> level);
    void update(float dt);
    void draw(sf::RenderTarget& target, sf::Vector2f centr) const;

private:
    std::vector<RopeNode> nodes;
    std::shared_ptr<Level> m_level;
};