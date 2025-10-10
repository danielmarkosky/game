#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include "level.h"

class Player {
public:
    Player(const sf::Vector2f& startPos, std::shared_ptr<Level> level);
    virtual ~Player() = default;

    virtual void update(float dt);
    virtual void draw(sf::RenderWindow& window);

    void moveLeft();
    void moveRight();
    void jump();
    void stopHorizontal();

    sf::Vector2f getPosition() const;
    sf::FloatRect getBounds() const;

protected:
    sf::RectangleShape shape;
    sf::Vector2f velocity;
    bool isJumping = false;
    std::shared_ptr<Level> level;
    float moveSpeed = 100.f;
    float jumpStrength = 200.f;
    float gravity = 400.f;

    bool canMove(const sf::Vector2f& offset) const;
};
