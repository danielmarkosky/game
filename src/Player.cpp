#include "Player.h"
#include "src/Config.h"
#include "src/utils.h"
#include <algorithm>
#include <cmath>

namespace
{
    // Turns a "multiply by this every 1/60 s" factor into one for a step of `dt` seconds.
    float decay(float perFrame, float dt)
    {
        return std::pow(perFrame, dt * 60.f);
    }

    float sign(float v)
    {
        return v < 0.f ? -1.f : 1.f;
    }

    // Frames in assets/gfx/player.png (12x14 each, one row per skin), made by tools/make_art.py.
    namespace frames
    {
        constexpr int WIDTH = 12;
        constexpr int HEIGHT = 14;
        constexpr int IDLE = 0;   // 2 frames
        constexpr int RUN = 2;    // 4 frames
        constexpr int JUMP = 6;
        constexpr int FALL = 7;
        constexpr int SWING = 8;
    }
}

Player::Player(std::shared_ptr<const Level> lvl)
    : level(std::move(lvl))
{
    respawn(level->getStartPos());
    texture.loadFromFile(config::assetPath("gfx/player.png"));
}

void Player::handleInput(const PlayerInput& in)
{
    input = in;

    if (in.jumpPressed) jumpBufferTimer = JUMP_BUFFER_TIME;

    if (!in.ropeHeld) ropeShotQueued = false;
    if (in.ropePressed || ropeShotQueued) {
        if (ropeCooldownTimer > 0.f) {
            ropeShotQueued = true;
        } else {
            ropeShotQueued = false;
            if (auto hit = ropeTarget(in.aim)) {
                rope.emplace(exactCentre(), *hit, level);
                events.push_back({PlayerEvent::Type::RopeAttached, *hit});
            }
        }
    }
    if (rope && !in.ropeHeld) {
        rope.reset();
        ropeJumpTimer = ROPE_JUMP_GRACE;
        ropeCooldownTimer = ROPE_COOLDOWN;
    }
}

void Player::update(float dt)
{
    const bool grounded = isGrounded();
    coyoteTimer = grounded ? COYOTE_TIME : std::max(0.f, coyoteTimer - dt);
    jumpBufferTimer = std::max(0.f, jumpBufferTimer - dt);
    dropThroughTimer = std::max(0.f, dropThroughTimer - dt);
    ropeJumpTimer = std::max(0.f, ropeJumpTimer - dt);
    ropeCooldownTimer = std::max(0.f, ropeCooldownTimer - dt);
    if (grounded && !rope && input.moveY < 0.f && level->isOnPlatform(position, WIDTH)) {
        dropThroughTimer = DROP_THROUGH_TIME;
    }

    const float inputX = input.moveX;
    if (inputX != 0.f) facingLeft = inputX < 0.f;

    const bool swinging = rope && !grounded;

    // --- Horizontal ---
    if (swinging) {
        // Pumping the swing: a free push in either direction, no run-speed cap.
        velocity.x += inputX * SWING_ACCELERATION * dt;
    } else {
        // Input can accelerate up to run speed, but never takes away speed you already have.
        if (inputX != 0.f && velocity.x * inputX < MAX_RUN_SPEED) {
            velocity.x += inputX * RUN_ACCELERATION * dt;
            if (velocity.x * inputX > MAX_RUN_SPEED) velocity.x = inputX * MAX_RUN_SPEED;
        }

        const bool overspeed = std::abs(velocity.x) > MAX_RUN_SPEED;
        if (grounded) {
            if (inputX == 0.f) {
                velocity.x *= decay(GROUND_DRAG, dt);
            } else if (overspeed) {
                // Landing from a fling: slide back down to run speed.
                velocity.x = sign(velocity.x) * std::max(MAX_RUN_SPEED, std::abs(velocity.x) * decay(GROUND_DRAG, dt));
            }
        } else {
            if (overspeed) velocity.x *= decay(OVERSPEED_AIR_DRAG, dt);
            else if (inputX == 0.f) velocity.x *= decay(AIR_DRAG, dt);
        }
        if (inputX == 0.f && std::abs(velocity.x) < 1.f) velocity.x = 0.f;
    }

    // --- Vertical ---
    if (velocity.y <= 0.f) jumping = false;
    float gravity = config::GRAVITY;
    if (jumping && !input.jumpHeld) gravity *= JUMP_RELEASE_GRAVITY;
    velocity.y -= gravity * dt;
    if (velocity.y < -MAX_FALL_SPEED) velocity.y = -MAX_FALL_SPEED;
    if (grounded && velocity.y < 0.f) {
        // Standing: don't let gravity pile up in the sub-pixel remainder.
        velocity.y = 0.f;
        remainder.y = 0.f;
    }

    if (jumpBufferTimer > 0.f) {
        if (coyoteTimer > 0.f) {
            velocity.y = JUMP_SPEED;
            jumping = true;
            jumpBufferTimer = 0.f;
            coyoteTimer = 0.f;
            squash = {0.75f, 1.3f};
            events.push_back({PlayerEvent::Type::Jumped, position + sf::Vector2f(WIDTH * 0.5f, 0.f)});
        } else if (rope || ropeJumpTimer > 0.f) {
            // Jumping off the rope lets go with a full jump, e.g. to hop up onto a ledge.
            rope.reset();
            velocity.y = std::max(velocity.y, ROPE_JUMP_SPEED);
            jumping = true;
            jumpBufferTimer = 0.f;
            ropeJumpTimer = 0.f;
            ropeCooldownTimer = ROPE_JUMP_COOLDOWN;
            squash = {0.75f, 1.3f};
        }
    }

    // --- Rope ---
    if (rope) {
        if (input.moveY != 0.f) rope->setLength(rope->length() - input.moveY * REEL_SPEED * dt);

        float speed = magnitude(velocity);
        if (swinging && speed > MAX_SWING_SPEED) velocity *= MAX_SWING_SPEED / speed;
        velocity = rope->constrainVelocity(exactCentre(), velocity);
    }

    const sf::Vector2f before = exactCentre();
    const float fallSpeed = -velocity.y;
    moveX(velocity.x * dt);
    moveY(velocity.y * dt);

    if (!grounded && isGrounded() && fallSpeed > 60.f) {
        float amount = std::min(1.f, fallSpeed / Player::MAX_FALL_SPEED);
        squash = {1.f + 0.4f * amount, 1.f - 0.35f * amount};
        events.push_back({PlayerEvent::Type::Landed, position + sf::Vector2f(WIDTH * 0.5f, 0.f), fallSpeed});
    }
    if (grounded && std::abs(velocity.x) > 60.f) {
        stepTimer -= dt;
        if (stepTimer <= 0.f) {
            stepTimer = 0.2f;
            events.push_back({PlayerEvent::Type::Stepped, position + sf::Vector2f(facingLeft ? WIDTH : 0.f, 0.f)});
        }
    }
    animTime += dt;
    squash += (sf::Vector2f(1.f, 1.f) - squash) * (1.f - std::exp(-14.f * dt));

    if (rope) {
        const size_t pivotsBefore = rope->pivots().size();
        rope->wrap(before, exactCentre());
        if (rope->pivots().size() > pivotsBefore) events.push_back({PlayerEvent::Type::RopeWrapped, rope->pivot()});
        // Moving in a straight line drifts slightly off the circle; pull back onto it.
        sf::Vector2f current = exactCentre();
        sf::Vector2f correction = rope->constrainPosition(current) - current;
        moveX(correction.x);
        moveY(correction.y);
        rope->update(centre(), dt);
    }
}

void Player::moveX(float amount)
{
    remainder.x += amount;
    int move = static_cast<int>(std::round(remainder.x));
    if (move == 0) return;
    remainder.x -= static_cast<float>(move);

    const int step = move > 0 ? 1 : -1;
    const bool airborne = !isGrounded();
    while (move != 0) {
        sf::Vector2f next = position + sf::Vector2f(static_cast<float>(step), 0.f);
        if (!fits(next)) {
            // In the air, a ledge lip just below your feet pops you up onto it instead of stopping you.
            bool popped = false;
            for (int up = 1; airborne && up <= CORNER_CORRECTION && !popped; ++up) {
                sf::Vector2f lifted(0.f, static_cast<float>(up));
                if (fits(position + lifted) && fits(next + lifted)) {
                    next += lifted;
                    popped = true;
                }
            }
            if (!popped) {
                velocity.x = 0.f;
                remainder.x = 0.f;
                return;
            }
        }
        position = next;
        move -= step;
    }
}

void Player::moveY(float amount)
{
    remainder.y += amount;
    int move = static_cast<int>(std::round(remainder.y));
    if (move == 0) return;
    remainder.y -= static_cast<float>(move);

    const int step = move > 0 ? 1 : -1;
    while (move != 0) {
        sf::Vector2f next = position + sf::Vector2f(0.f, static_cast<float>(step));
        if (step < 0 && dropThroughTimer <= 0.f && level->isOnPlatform(position, WIDTH)) {
            velocity.y = 0.f;
            remainder.y = 0.f;
            return;
        }
        if (!fits(next)) {
            // Clipping a corner with your head nudges you sideways around it instead of bonking.
            bool nudged = false;
            const float preferred = velocity.x < 0.f ? -1.f : 1.f;
            for (int side = 1; step > 0 && side <= CORNER_CORRECTION && !nudged; ++side) {
                for (float dir : {preferred, -preferred}) {
                    sf::Vector2f shift(dir * static_cast<float>(side), 0.f);
                    if (fits(position + shift) && fits(next + shift)) {
                        next += shift;
                        nudged = true;
                        break;
                    }
                }
            }
            if (!nudged) {
                velocity.y = 0.f;
                remainder.y = 0.f;
                jumping = false;
                return;
            }
        }
        position = next;
        move -= step;
    }
}

bool Player::fits(sf::Vector2f pos) const
{
    return level->isEmptySpace(pos, HEIGHT, WIDTH);
}

void Player::draw(sf::RenderTarget& target, sf::Vector2f centr) const
{
    if (rope) rope->draw(target, centr);

    int frame;
    if (!isGrounded()) {
        frame = rope ? frames::SWING : velocity.y > 0.f ? frames::JUMP : frames::FALL;
    } else if (std::abs(velocity.x) > 5.f) {
        frame = frames::RUN + static_cast<int>(animTime * 12.f) % 4;
    } else {
        frame = frames::IDLE + static_cast<int>(animTime * 2.f) % 2;
    }

    sf::Sprite sprite(texture);
    sprite.setTextureRect(sf::IntRect(frame * frames::WIDTH, static_cast<int>(skin) * frames::HEIGHT, frames::WIDTH, frames::HEIGHT));
    // Anchored at the middle of the feet, so squashing and flipping keep the player planted.
    // The frames have a 1px outline below the feet, so they sit one pixel lower.
    sprite.setOrigin(frames::WIDTH * 0.5f, frames::HEIGHT - 1.f);
    sprite.setScale(facingLeft ? -squash.x : squash.x, squash.y);
    sprite.setPosition(viewCoordinates(position + sf::Vector2f(WIDTH * 0.5f, 0.f), centr));
    target.draw(sprite);
}

void Player::setSkin(size_t index)
{
    skin = std::min(index, SKINS.size() - 1);
}

void Player::drawPreview(sf::RenderTarget& target, sf::Vector2f feet, float scale, float time) const
{
    const int frame = frames::RUN + static_cast<int>(time * 12.f) % 4;
    sf::Sprite sprite(texture);
    sprite.setTextureRect(sf::IntRect(frame * frames::WIDTH, static_cast<int>(skin) * frames::HEIGHT, frames::WIDTH, frames::HEIGHT));
    sprite.setOrigin(frames::WIDTH * 0.5f, frames::HEIGHT - 1.f);
    sprite.setScale(scale, scale);
    sprite.setPosition(feet);
    target.draw(sprite);
}

void Player::respawn(sf::Vector2f pos)
{
    position = {std::round(pos.x), std::round(pos.y)};
    remainder = {0.f, 0.f};
    velocity = {0.f, 0.f};
    rope.reset();
    coyoteTimer = 0.f;
    jumpBufferTimer = 0.f;
    dropThroughTimer = 0.f;
    ropeJumpTimer = 0.f;
    ropeCooldownTimer = 0.f;
    ropeShotQueued = false;
    squash = {1.f, 1.f};
    jumping = false;
    events.clear();
}

sf::Vector2f Player::centre() const
{
    return position + sf::Vector2f(WIDTH * .5f, HEIGHT * .5f);
}

sf::Vector2f Player::exactCentre() const
{
    return centre() + remainder;
}

sf::Vector2f Player::getPosition() const { return position; }

sf::Vector2f Player::getVelocity() const { return velocity; }

void Player::setVelocity(sf::Vector2f v) { velocity = v; }

bool Player::isGrounded() const
{
    if (!level->isEmptySpace(position + sf::Vector2f(0.f, -1.f), 1.f, WIDTH)) return true;
    return dropThroughTimer <= 0.f && level->isOnPlatform(position, WIDTH);
}

bool Player::isTouchingDanger() const
{
    // A pixel of leeway on every side, so grazing a hazard doesn't kill.
    return level->isDangerousSpace(position + sf::Vector2f(1.f, 1.f), HEIGHT - 2.f, WIDTH - 2.f);
}

const Rope* Player::getRope() const
{
    return rope ? &*rope : nullptr;
}

std::vector<PlayerEvent> Player::takeEvents()
{
    std::vector<PlayerEvent> taken;
    taken.swap(events);
    return taken;
}

std::optional<sf::Vector2f> Player::ropeTarget(sf::Vector2f aim) const
{
    return nearestSolidPoint(exactCentre(), aim, level, Rope::MIN_LENGTH, ROPE_RANGE);
}

bool Player::canShootRope() const
{
    return ropeCooldownTimer <= 0.f;
}
