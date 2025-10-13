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

    void handleEvent(const sf::Event& event); // Handles keyboard events

    sf::Vector2f getPosition() const;
    sf::FloatRect getBounds() const;

protected:
    sf::RectangleShape shape;
    sf::Vector2f velocity;
    bool isJumping = false;
    bool isOnGround = false; // Track if player is on ground
    std::shared_ptr<Level> level;
    float moveSpeed;
    float jumpStrength;
    float gravity;

    bool canMove(const sf::Vector2f& offset) const;
    // Player parameters are not initialized here, but in derived classes
};
