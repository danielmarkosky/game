#include "player.h"
#include <algorithm>

Player::Player(const sf::Vector2f& startPos, std::shared_ptr<Level> level)
    : level(std::move(level)) {}

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
    float prevVelocityY = velocity.y;
    isOnGround = false; // Reset before collision check
    if (!canMove(sf::Vector2f(0, velocity.y * dt))) {
        // If falling and hit ground, set isOnGround true
        if (prevVelocityY > 0) {
            isOnGround = true;
            isJumping = false;
            // Snap player to ground
        }
        velocity.y = 0;
    }
    // Move player
    shape.move(velocity * dt);
}

void Player::draw(sf::RenderWindow& window) {
    window.draw(shape);
}

void Player::moveLeft() {
    // Only move left if no wall
    if (canMove(sf::Vector2f(-moveSpeed * 0.016f, 0))) { // 0.016f ~ 60 FPS frame
        velocity.x = -moveSpeed;
    } else {
        velocity.x = 0;
    }
}

void Player::moveRight() {
    // Only move right if no wall
    if (canMove(sf::Vector2f(moveSpeed * 0.016f, 0))) {
        velocity.x = moveSpeed;
    } else {
        velocity.x = 0;
    }
}

void Player::jump() {
    if (isOnGround) {
        velocity.y = -jumpStrength;
        isJumping = true;
        isOnGround = false;
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

void Player::handleEvent(const sf::Event& event) {
    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::A) moveLeft();
        if (event.key.code == sf::Keyboard::D) moveRight();
        if (event.key.code == sf::Keyboard::Space) jump();
    }
    if (event.type == sf::Event::KeyReleased) {
        if (event.key.code == sf::Keyboard::A || event.key.code == sf::Keyboard::D) {
            stopHorizontal();
        }
    }
}
