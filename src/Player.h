#pragma once

#include "src/Level.h"

#include <SFML/Graphics.hpp>
#include <memory>

class Player
{
public:
    explicit Player(const sf::Vector2f& startPos, std::shared_ptr<const Level> lvl);
    void draw(sf::RenderTarget& target) const;
    void update(float dt);
    sf::Vector2f centre() const;

protected:
    sf::Vector2f position;
    sf::Vector2f velocity;
    float width = 7.f;
    float height = 11.f;
    sf::Texture texture;
    sf::Sprite sprite;
    std::shared_ptr<const Level> level;
};
