#include "Player.h"

#include "src/utils.h"
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>

Player::Player(std::shared_ptr<const Level> lvl)
    : level(std::move(lvl)), velocity(50.f, 50.f)
{
	position = level->getStartPos();
    if (!texture.loadFromFile("assets/temp_player.png")) {
        throw std::runtime_error("No image porfawor");
    }
    sprite.setTexture(texture);
    sprite.setTextureRect(sf::IntRect(0, 0, 9, 12));
    auto p0 = viewCoordinates(position + sf::Vector2f(-1, 12), centre());
    sprite.setPosition(p0);
}

void Player::draw(sf::RenderTarget& target) const
{
    target.draw(sprite);
}

void Player::update(float dt)
{
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
        velocity.x = -moveSpeed;
    } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
        velocity.x = moveSpeed;
    } else {
        velocity.x = 0.f;
    }

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space) and canJump()) velocity.y = jumpSpeed;

    velocity.y -= gravity * dt;
    if (velocity.y < maxFallSpeed) velocity.y = maxFallSpeed;

    sf::Vector2f nextPos;

    nextPos = position + sf::Vector2f(velocity.x, 0.f) * dt;
    if (level->isEmptySpace(nextPos, height, width)) {
        position = nextPos;
    } else {
        velocity.x = 0.f;
    }

    nextPos = position + sf::Vector2f(0.f, velocity.y) * dt;
    if (level->isEmptySpace(nextPos, height, width)) {
        position = nextPos;
    } else {
        velocity.y = 0.f;
    }

    if (level->isDangerousSpace(position, height, width)) {
        resetPlayer();
    }

    auto p = viewCoordinates(position + sf::Vector2f(-1.f, 12.f), centre());
    sprite.setPosition(std::floor(p.x), std::floor(p.y));
}

sf::Vector2f Player::centre() const
{
    return position + sf::Vector2f(width * .5f, height * .5f);
}

void Player::resetPlayer()
{
    position = this->level->getStartPos();
    velocity = sf::Vector2f(0.f, 0.f);
}

bool Player::canJump()
{
    sf::Vector2f belowPos = position + sf::Vector2f(0.f, -1.f);
    return not level->isEmptySpace(belowPos, 1.f, width);
}