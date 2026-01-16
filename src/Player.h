#pragma once

#include "src/Level.h"
#include "src/Rope.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include "InputManager.h"

class Player
{
public:
    explicit Player(std::shared_ptr<Level> lvl, std::shared_ptr<InputManager> inputManager);
    void draw(sf::RenderTarget& target) const;
    void update(float dt);
    sf::Vector2f centre() const;
    void resetPlayer();
    void handleShootRope();

protected:
    sf::Vector2f position;
    sf::Vector2f velocity;

    // Dimensions
    const float width = 7.f;
    const float height = 11.f;

    // Movement Physics
    const float runAcceleration = 800.f;  // Force applied when running
    const float maxRunSpeed = 90.f;       // Max horizontal speed running
    const float jumpSpeed = 235.f;
    const float gravity = 890.f;
    const float maxFallSpeed = -380.f;

    // Damping (Friction)
    const float groundDrag = 0.85f; // Stops quickly on ground
    const float airDrag = 0.98f;    // Slowly loses momentum in air (requested feature)

    sf::Texture texture;
    sf::Sprite sprite;
    std::shared_ptr<const Level> level;
    std::shared_ptr<Rope> rope;
    std::shared_ptr<InputManager> inputManager;
    bool canJump();
};
