#include "Player.h"
#include "src/utils.h"
#include "src/InputManager.h"
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>

Player::Player(std::shared_ptr<Level> lvl, std::shared_ptr<InputManager> inMan)
    : level(lvl), velocity(0.f, 0.f), inputManager(inMan)
{
    position = level->getStartPos();
    if (!texture.loadFromFile("assets/temp_player.png")) {
        // throw std::runtime_error("No image");
    }
    sprite.setTexture(texture);
    sprite.setTextureRect(sf::IntRect(0, 0, 9, 12));
    auto p0 = viewCoordinates(position + sf::Vector2f(-1, 12), centre());
    sprite.setPosition(p0);
}

void Player::draw(sf::RenderTarget& target) const
{
    target.draw(sprite);
    if (rope) {
        rope->draw(target, centre());
    }
}

void Player::update(float dt)
{
    handleShootRope();

    float inputX = 0.f;
    if (inputManager->isKeyPressed(sf::Keyboard::A)) inputX = -1.f;
    if (inputManager->isKeyPressed(sf::Keyboard::D)) inputX = 1.f;

    if (rope) {
        // --- ROPE SWING STATE ---

        // Check ground friction logic
        sf::Vector2f below = position + sf::Vector2f(0.f, -1.f);
        bool isGrounded = !level->isEmptySpace(below, 1.f, width);

        if (isGrounded) {
             // 1. GROUNDED LOGIC: WALK NORMALLY
             // We manipulate the rope node velocity directly to match standard walking physics
             sf::Vector2f currentVel = rope->getNodeVelocity(0, dt);

             if (inputX != 0.f) {
                 // Accelerate
                 currentVel.x += inputX * runAcceleration * dt;
                 // Clamp Speed
                 if (currentVel.x > maxRunSpeed) currentVel.x = maxRunSpeed;
                 if (currentVel.x < -maxRunSpeed) currentVel.x = -maxRunSpeed;
             } else {
                 // Friction
                 currentVel.x *= groundDrag;
                 if (std::abs(currentVel.x) < 1.0f) currentVel.x = 0.f;
             }

             // Apply walking velocity
             rope->setNodeVelocity(0, currentVel, dt);

             if (inputManager->wasKeyJustPressed(sf::Keyboard::Space)) {
                 // Jump
                 currentVel = rope->getNodeVelocity(0, dt);
                 currentVel.y = jumpSpeed;
                 rope->setNodeVelocity(0, currentVel, dt);
             }
        } else {
             // 2. AIRBORNE LOGIC: SWING
             if (inputX != 0.f) {
                // Apply 'soft' swing force
                rope->applyForceToNode(0, sf::Vector2f(inputX * 150.f * dt * dt, 0.f));
             }
        }

        // 3. Physics Update
        rope->update(dt);

        // 4. Resolve Collision (Player vs World)
        sf::Vector2f nodePos = rope->getNodePosition(0);

        // --- ANTI-SLIDE FIX ---
        if (isGrounded && inputX == 0.f) {
            sf::Vector2f currentCenter = centre();
            // Stronger snap to prevent micro-sliding while idle on rope
            nodePos.x = currentCenter.x + (nodePos.x - currentCenter.x) * 0.01f;
        }

        sf::Vector2f targetPos = nodePos - sf::Vector2f(width * 0.5f, height * 0.5f);

        sf::Vector2f moveDelta = targetPos - position;
        sf::Vector2f nextPos;

        // Try X Movement
        nextPos = position + sf::Vector2f(moveDelta.x, 0.f);
        if (level->isEmptySpace(nextPos, height, width)) {
            position = nextPos;
        } else {
            moveDelta.x = 0.f; // Wall hit
        }

        // Try Y Movement
        nextPos = position + sf::Vector2f(0.f, moveDelta.y);
        if (level->isEmptySpace(nextPos, height, width)) {
            position = nextPos;
        } else {
            moveDelta.y = 0.f; // Ceiling/Floor hit
        }

        // 5. Feedback Collision to Rope
        if (std::abs(moveDelta.x - (targetPos.x - position.x)) > 0.1f ||
            std::abs(moveDelta.y - (targetPos.y - position.y)) > 0.1f)
        {
            rope->setNodePosition(0, centre());
        }

        // 6. Sync Velocity
        velocity = rope->getNodeVelocity(0, dt);

        if (inputManager->wasMouseReleased()) {
             rope = nullptr;
        }
    }
    else {
        // --- FREE MOVEMENT STATE ---
        velocity.x += inputX * runAcceleration * dt;
        velocity.y -= gravity * dt;

        bool grounded = canJump();

        if (inputX == 0.f) {
            float drag = grounded ? groundDrag : airDrag;
            velocity.x *= drag;
            if (std::abs(velocity.x) < 1.0f) velocity.x = 0.f;
        } else {
            if (velocity.x > maxRunSpeed) velocity.x = maxRunSpeed;
            if (velocity.x < -maxRunSpeed) velocity.x = -maxRunSpeed;
        }

        if (velocity.y < maxFallSpeed) velocity.y = maxFallSpeed;

        if (inputManager->isKeyPressed(sf::Keyboard::Space) && grounded) {
            velocity.y = jumpSpeed;
        }

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
    }

    if (level->isDangerousSpace(position, height, width)) {
        resetPlayer();
        rope = nullptr;
    }

    auto p = viewCoordinates(position + sf::Vector2f(-1.f, 12.f), centre());
    sprite.setPosition(std::floor(p.x), std::floor(p.y));
}

sf::Vector2f Player::centre() const {
    return position + sf::Vector2f(width * .5f, height * .5f);
}

void Player::resetPlayer() {
    position = this->level->getStartPos();
    velocity = sf::Vector2f(0.f, 0.f);
}

bool Player::canJump() {
    if (rope) return true;
    sf::Vector2f belowPos = position + sf::Vector2f(0.f, -1.f);
    return not level->isEmptySpace(belowPos, 1.f, width);
}

void Player::handleShootRope() {
    if (inputManager->wasMouseClicked()) {
        sf::Vector2f screenPos = inputManager->mousePosition();
        sf::Vector2f targetWorldPos = worldCoordinates(screenPos, centre());
        sf::Vector2f startPos = centre();

        auto hitPoint = nearestSolidPoint(startPos, targetWorldPos, level);

        if (hitPoint) {
            auto mutableLevel = std::const_pointer_cast<Level>(level);
            rope = std::make_shared<Rope>(startPos, *hitPoint, mutableLevel);
        }
    }
}
