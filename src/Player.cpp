#include "Player.h"

#include "src/utils.h"
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>

// Player::Player(const sf::Vector2f& startPos, std::shared_ptr<const Level> lvl)
//     : position(startPos), level(std::move(lvl)), velocity(10.f, 10.f)
// {
//     if (!texture.loadFromFile("assets/player.png")) {
//         throw std::runtime_error("No image porfawor");
//     }
//     sprite.setTexture(texture);
//     sprite.setPosition(viewCoordinates(position, centre()));
// }

Player::Player(const sf::Vector2f& startPos, std::shared_ptr<const Level> lvl)
    : position(startPos), level(std::move(lvl)), velocity(10.f, 10.f)
{
    if (!texture.loadFromFile("assets/player.png")) {
        throw std::runtime_error("No image porfawor");
    }
    sprite.setTexture(texture);
    sprite.setOrigin(width * 0.5f, height * 0.5f);
    auto p = viewCoordinates(position, centre());
    sprite.setPosition(std::floor(p.x + 0.5f), std::floor(p.y + 0.5f));
}

void Player::draw(sf::RenderTarget& target) const
{
    target.draw(sprite);
}

void Player::update(float dt)
{
    sf::Vector2f nextPos;
    nextPos = position + sf::Vector2f(velocity.x, 0) * dt;
    if (level->isEmptySpace(nextPos, height, width)) position = nextPos;
    nextPos = position + sf::Vector2f(0, velocity.y) * dt;
    if (level->isEmptySpace(nextPos, height, width)) position = nextPos;
    auto p = viewCoordinates(position, centre());
    sprite.setPosition(std::floor(p.x + 0.5f), std::floor(p.y + 0.5f));
}

sf::Vector2f Player::centre() const
{
    return position + sf::Vector2f(width * .5f, height * .5f);
}
