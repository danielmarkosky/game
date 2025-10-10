#include "player.h"
#include <algorithm>

Player::Player(const sf::Vector2f& startPos, std::shared_ptr<Level> level)
    : level(std::move(level))
{
    shape.setSize(sf::Vector2f(16.f, 24.f)); // Example size
    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineThickness(2.f);
    shape.setOutlineColor(sf::Color::Green);
    shape.setPosition(startPos);

    jumpStrength = 320.f; // Increased jump strength for higher jumps
}

void Player::update(float dt) {
    // Apply gravity
    velocity.y += gravity * dt;

    // Predict next position
    sf::Vector2f nextPos = shape.getPosition() + velocity * dt;
    sf::FloatRect nextBounds(nextPos, shape.getSize());

    // Check collision with walls
    if (!canMove(sf::Vector2f(velocity.x * dt, 0))) {
        velocity.x = 0;
    }
    if (!canMove(sf::Vector2f(0, velocity.y * dt))) {
        velocity.y = 0;
        isJumping = false;
    }

    // Move player
    shape.move(velocity * dt);
}

void Player::draw(sf::RenderWindow& window) {
    window.draw(shape);
}

void Player::moveLeft() {
    velocity.x = -moveSpeed;
}

void Player::moveRight() {
    velocity.x = moveSpeed;
}

void Player::jump() {
    if (!isJumping) {
        velocity.y = -jumpStrength;
        isJumping = true;
    }
}

void Player::stopHorizontal() {
    velocity.x = 0;
}

sf::Vector2f Player::getPosition() const {
    return shape.getPosition();
}

sf::FloatRect Player::getBounds() const {
    return shape.getGlobalBounds();
}

bool Player::canMove(const sf::Vector2f& offset) const {
    sf::FloatRect nextBounds = shape.getGlobalBounds();
    nextBounds.left += offset.x;
    nextBounds.top += offset.y;
    for (const auto& wall : level->getWalls()) {
        if (nextBounds.intersects(wall.getRect())) { // Use wall.getRect() to get sf::FloatRect
            return false;
        }
    }
    return true;
}
