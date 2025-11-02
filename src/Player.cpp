#include "Player.h"

#include "src/utils.h"
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>

Player::Player(const sf::Vector2f& startPos, std::shared_ptr<const Level> lvl)
    : position(startPos), level(std::move(lvl)), velocity(50.f, 50.f)
{
    if (!texture.loadFromFile("assets/player.png")) {
        throw std::runtime_error("No image porfawor");
    }
    sprite.setTexture(texture);
    sprite.setOrigin(0.f, height);
    auto p0 = viewCoordinates(position, centre());
    sprite.setPosition(std::floor(p0.x + 0.5f), std::floor(p0.y + 0.5f));
}

void Player::draw(sf::RenderTarget& target) const
{
    target.draw(sprite);
}

void Player::update(float dt)
{
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) velocity.x = (-1) * moveSpeed;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) velocity.x = moveSpeed;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space) and canJump()) velocity.y = jumpSpeed;

    velocity.y -= gravity * dt;
    if (velocity.y < maxFallSpeed) velocity.y = maxFallSpeed;

    sf::Vector2f nextPos;

    nextPos = position + sf::Vector2f(velocity.x, 0.f) * dt;
    if (level->isEmptySpace(nextPos, height, width)) {
        position = nextPos;
    }
    velocity.x = 0.f;

    nextPos = position + sf::Vector2f(0.f, velocity.y) * dt;
    if (level->isEmptySpace(nextPos, height, width)) {
        position = nextPos;
    } else {
        velocity.y = 0.f;
    }

    auto p = viewCoordinates(position, centre());
    sprite.setPosition(std::floor(p.x + 0.5f), std::floor(p.y + 0.5f));
}

sf::Vector2f Player::centre() const
{
    return position + sf::Vector2f(width * .5f, height * .5f);
}

bool Player::canJump()
{
    sf::Vector2f belowPos = position + sf::Vector2f(0.f, -1.f);
    return not level->isEmptySpace(belowPos, 1.f, width);
}