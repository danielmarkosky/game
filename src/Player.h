#pragma once

#include "src/Level.h"
#include "src/Rope.h"

#include <SFML/Graphics.hpp>
#include <memory>

#include "InputManager.h"

class Player
{
public:
    explicit Player(std::shared_ptr<const Level> lvl);
    void draw(sf::RenderTarget& target) const;
    void update(float dt);
    sf::Vector2f centre() const;
    void resetPlayer();

protected:
    sf::Vector2f position;
    sf::Vector2f velocity;
    const float width = 7.f;
    const float height = 11.f;
    const float moveSpeed = 100.f;
    const float jumpSpeed = 300.f;
    const float gravity = 900.f;
    const float maxFallSpeed = (-1) * 900.f;
    sf::Texture texture;
    sf::Sprite sprite;
    std::shared_ptr<const Level> level;
    std::unique_ptr<Rope> rope;
    std::unique_ptr<InputManager> inputManager;
    enum class AnimState {
        Idle,
        Moving,
        Jumping
    } state = AnimState::Idle;
    bool canJump();
};
