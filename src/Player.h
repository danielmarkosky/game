#pragma once

#include "src/Level.h"
#include "src/Rope.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <vector>

// What the player wants to do this frame, already translated from keys and mouse.
struct PlayerInput
{
    float moveX = 0.f;         // -1 left, 0, 1 right
    float moveY = 0.f;         // 1 reels the rope in, -1 lets it out (or drops through a platform)
    bool jumpPressed = false;  // went down this frame
    bool jumpHeld = false;
    bool ropePressed = false;  // went down this frame
    bool ropeHeld = false;
    sf::Vector2f aim;          // world position the rope is shot towards
};

// Something that happened during an update, so the game can show an effect for it.
struct PlayerEvent
{
    enum class Type { Jumped, Landed, Stepped, RopeAttached, RopeWrapped };
    Type type;
    sf::Vector2f position;
    float strength = 0.f; // Landed: how fast the player was falling
};

class Player
{
public:
    explicit Player(std::shared_ptr<const Level> lvl);

    // Once per rendered frame: reacts to button presses (shooting and releasing the rope, buffering jumps).
    void handleInput(const PlayerInput& input);
    // Once per fixed physics step.
    void update(float dt);
    void draw(sf::RenderTarget& target, sf::Vector2f centr) const;

    void respawn(sf::Vector2f pos);

    sf::Vector2f centre() const;
    // Bottom-left corner of the hitbox. Always whole pixels.
    sf::Vector2f getPosition() const;
    sf::Vector2f getVelocity() const;
    void setVelocity(sf::Vector2f v);
    bool isGrounded() const;
    bool isTouchingDanger() const;
    const Rope* getRope() const;
    // Where a rope shot towards `aim` would attach, if anywhere.
    std::optional<sf::Vector2f> ropeTarget(sf::Vector2f aim) const;
    // Events since the last call.
    std::vector<PlayerEvent> takeEvents();

    // Dimensions
    static constexpr float WIDTH = 7.f;
    static constexpr float HEIGHT = 11.f;

    // Movement physics
    static constexpr float RUN_ACCELERATION = 800.f;
    static constexpr float MAX_RUN_SPEED = 90.f;
    static constexpr float JUMP_SPEED = 235.f;
    static constexpr float MAX_FALL_SPEED = 380.f;
    static constexpr float JUMP_RELEASE_GRAVITY = 3.f;  // gravity multiplier when jump is let go early
    static constexpr float COYOTE_TIME = 0.1f;          // can still jump this long after leaving a ledge
    static constexpr float JUMP_BUFFER_TIME = 0.1f;     // a jump pressed this long before landing still counts

    // Damping per 1/60 s, applied the same at any frame rate
    static constexpr float GROUND_DRAG = 0.85f;         // stops quickly on ground
    static constexpr float AIR_DRAG = 0.98f;            // slowly loses momentum in air
    static constexpr float OVERSPEED_AIR_DRAG = 0.995f; // above run speed (after a swing) momentum lasts longer

    // Rope
    static constexpr float ROPE_RANGE = 100.f;
    static constexpr float SWING_ACCELERATION = 300.f;
    static constexpr float MAX_SWING_SPEED = 320.f;
    static constexpr float REEL_SPEED = 70.f;
    static constexpr float ROPE_JUMP_SPEED = JUMP_SPEED; // jumping off the rope is a full jump
    static constexpr float ROPE_JUMP_GRACE = 0.1f;       // ...even just after letting go of it

    // How far the player gets nudged sideways around a corner instead of bonking into it.
    static constexpr int CORNER_CORRECTION = 4;
    // Holding down on a one-way platform drops through it for this long.
    static constexpr float DROP_THROUGH_TIME = 0.15f;

private:
    // Pixel-by-pixel movement, so the player always ends up exactly touching what it hits.
    void moveX(float amount);
    void moveY(float amount);
    bool fits(sf::Vector2f pos) const;
    sf::Vector2f exactCentre() const;

    sf::Vector2f position;   // whole pixels
    sf::Vector2f remainder;  // sub-pixel movement not applied yet
    sf::Vector2f velocity;

    std::shared_ptr<const Level> level;
    std::optional<Rope> rope;
    sf::Texture texture;

    PlayerInput input;
    float coyoteTimer = 0.f;
    float jumpBufferTimer = 0.f;
    float dropThroughTimer = 0.f;
    float ropeJumpTimer = 0.f; // time left to jump off a rope that was just let go of
    bool jumping = false;
    bool facingLeft = false;

    // Looks
    std::vector<PlayerEvent> events;
    float animTime = 0.f;
    float stepTimer = 0.f;
    sf::Vector2f squash{1.f, 1.f}; // sprite scale, eased back to 1 after jumps and landings
};
