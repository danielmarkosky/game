#include "player.h"
#include <algorithm>

Player::Player(const sf::Vector2f& startPos, std::shared_ptr<Level> level)
    : level(std::move(level)), moveSpeed(200.f), jumpStrength(350.f), gravity(900.f) {
    // Load texture (replace with your texture path)
    if (!texture.loadFromFile("assets/duckpngsp.png")) {
        // Fallback: create a colored rectangle if texture fails
        sf::Image fallback;
        fallback.create(32, 32, sf::Color::Yellow);
        texture.loadFromImage(fallback);
    }
    sprite.setTexture(texture);
    sprite.setPosition(startPos);
}

void Player::update(float dt) {
    velocity.y += gravity * dt;
    sf::Vector2f nextPos = sprite.getPosition() + velocity * dt;
    sf::FloatRect nextBounds = sprite.getGlobalBounds();
    nextBounds.left = nextPos.x;
    nextBounds.top = nextPos.y;

    if (!canMove(sf::Vector2f(velocity.x * dt, 0))) {
        velocity.x = 0;
    }
    float prevVelocityY = velocity.y;
    isOnGround = false;
    if (!canMove(sf::Vector2f(0, velocity.y * dt))) {
        if (prevVelocityY > 0) {
            isOnGround = true;
            isJumping = false;
        }
        velocity.y = 0;
    }
    sprite.move(velocity * dt);
}

void Player::draw(sf::RenderWindow& window) {
    window.draw(sprite);
}

void Player::moveLeft() {
    if (canMove(sf::Vector2f(-moveSpeed * 0.016f, 0))) {
        velocity.x = -moveSpeed;
    } else {
        velocity.x = 0;
    }
}

void Player::moveRight() {
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

sf::Vector2f Player::getPosition() const {
    return sprite.getPosition();
}

sf::FloatRect Player::getBounds() const {
    return sprite.getGlobalBounds();
}

bool Player::canMove(const sf::Vector2f& offset) const {
    sf::FloatRect nextBounds = sprite.getGlobalBounds();
    nextBounds.left += offset.x;
    nextBounds.top += offset.y;
    // Check collision with level walls
    return !level->collidesWithWall(nextBounds);
}
