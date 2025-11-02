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
    const float width = 7.f;
    const float height = 11.f;
    const float moveSpeed = 100.f;
    const float jumpSpeed = 380.f;
    const float gravity = 1100.f;
    const float maxFallSpeed = (-1) * 900.f;
    bool canJump = false;
    sf::Texture texture;
    sf::Sprite sprite;
    std::shared_ptr<const Level> level;
};
