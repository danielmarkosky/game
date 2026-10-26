#include "Player.h"
#include "src/Config.h"
#include "src/Physics.h"
#include "src/utils.h"
#include <algorithm>
#include <cmath>

namespace
{
    // `value` moved towards `target` by at most `step`.
    float approach(float value, float target, float step)
    {
        return value < target ? std::min(value + step, target) : std::max(value - step, target);
    }

    float sign(float v)
    {
        return v < 0.f ? -1.f : 1.f;
    }

    // Frames in assets/gfx/player.png (12x14 each, one row per look), made by tools/make_art.py.
    namespace frames
    {
        constexpr int WIDTH = 12;
        constexpr int HEIGHT = 14;
        constexpr int IDLE = 0;   // 2 frames
        constexpr int RUN = 2;    // 4 frames
        constexpr int JUMP = 6;
        constexpr int FALL = 7;
        constexpr int SWING = 8;
        constexpr int SIT = 9;
        constexpr int CLIMB = 10; // 2 frames
    }

    const sf::Color ROPE_COLOUR(200, 190, 170);
    const sf::Color HOOK_COLOUR(255, 90, 90);

    // Grabbing a wall in the air only once a jump has slowed to this, so jumping up beside a wall
    // doesn't stop dead on it.
    constexpr float GRAB_BELOW_SPEED = 100.f;
    // Leaping off a wall uses this much of the strength for holding on.
    constexpr float WALL_JUMP_STAMINA = 0.5f;
}

Player::Player(std::shared_ptr<const Level> lvl)
    : level(std::move(lvl)), ropeRange(physics().ropeRange)
{
    respawn(level->getStartPos());
    texture.loadFromFile(config::assetPath("gfx/player.png"));
}

void Player::handleInput(const PlayerInput& in)
{
    input = in;
    if (in.jumpPressed) jumpBufferTimer = physics().jumpBufferTime;
    if (in.ropePressed) ropeShotQueued = true;
    if (!in.ropeHeld) {
        ropeShotQueued = false;
        if (rope) {
            letGoOfRope();
            ropeJumpTimer = ROPE_JUMP_GRACE;
        }
        // Let go before it caught: it's reeled back in.
        if (hook == Hook::WindingUp) hook = Hook::InHand;
        if (hook == Hook::Out) hook = Hook::Back;
    }
}

void Player::update(float dt)
{
    const Physics& p = physics();
    const bool grounded = isGrounded();
    if (grounded) {
        airJumped = false;
        stamina = p.climbStamina;
    }
    // In water: how deep your feet are under the surface.
    const bool wasSwimming = swimming;
    swimming = isSwimming();
    const float depth = swimming ? *level->depthInWater({position.x + WIDTH * 0.5f, position.y + 0.5f}) + 0.5f : 0.f;
    if (swimming && !wasSwimming) {
        events.push_back({PlayerEvent::Type::Splashed, position + sf::Vector2f(WIDTH * 0.5f, depth), std::max(0.f, -velocity.y)});
    }
    if (swimming) {
        airJumped = false;
        riding = 0; // a cart doesn't float
    }
    // Holding your breath: under water the air runs out, bubbles rising as it goes; at the surface
    // it comes back quickly.
    const bool wasUnderwater = underwater;
    underwater = isUnderwater();
    if (underwater) {
        air = std::max(0.f, air - dt);
        bubbleTimer -= dt;
        if (bubbleTimer <= 0.f) {
            bubbleTimer = airLeft() < 0.25f ? 0.25f : 0.8f;
            events.push_back({PlayerEvent::Type::Bubbled, position + sf::Vector2f(WIDTH * 0.5f + (facingLeft ? -2.f : 2.f), MOUTH_HEIGHT)});
        }
    } else {
        if (wasUnderwater && airLeft() < 0.5f) events.push_back({PlayerEvent::Type::Gasped, position + sf::Vector2f(WIDTH * 0.5f, MOUTH_HEIGHT)});
        air = std::min(maxAir(), air + dt * maxAir() / std::max(0.01f, p.airRefillTime));
        bubbleTimer = 0.3f;
    }
    coyoteTimer = grounded && !swimming ? p.coyoteTime : swimming ? 0.f : std::max(0.f, coyoteTimer - dt);
    jumpBufferTimer = std::max(0.f, jumpBufferTimer - dt);
    dropThroughTimer = std::max(0.f, dropThroughTimer - dt);
    ropeJumpTimer = std::max(0.f, ropeJumpTimer - dt);
    regrabTimer = std::max(0.f, regrabTimer - dt);
    if (grounded && !rope && !riding && input.moveY < 0.f && level->isOnPlatform(position, WIDTH)) {
        dropThroughTimer = DROP_THROUGH_TIME;
    }
    const bool floating = isFloating();

    const float inputX = input.moveX;
    if (inputX != 0.f) facingLeft = inputX < 0.f;

    updateThrow(dt, grounded);
    updateClimbing(dt, grounded);
    const bool swinging = rope && !grounded;

    // --- Horizontal ---
    if (floating) {
        // Pushed about by the arrows, any way, drifting to a stop with nothing held. On the rope,
        // up and down reel it in and out instead.
        const sf::Vector2f push(inputX, rope ? 0.f : input.moveY);
        for (auto [speed, along] : {std::pair{&velocity.x, push.x}, std::pair{&velocity.y, push.y}}) {
            if (along != 0.f) *speed = approach(*speed, along * p.floatSpeed, p.floatAcceleration * dt);
            else *speed -= *speed * std::min(1.f, p.floatDrag * dt);
        }
        strokeTimer -= dt;
        if (push != sf::Vector2f() && strokeTimer <= 0.f) {
            strokeTimer = 0.5f;
            events.push_back({PlayerEvent::Type::Pushed, exactCentre() - push * 4.f});
        }
    } else if (riding) {
        velocity.x = static_cast<float>(riding) * p.cartSpeed;
    } else if (clinging) {
        // Held still against the wall by updateClimbing.
    } else if (swinging) {
        // Pushing the swing along, like legs on a playground swing: only below where the rope hangs
        // from, never up and over the top. Most of the speed comes from falling.
        if (exactCentre().y < rope->pivot().y) velocity.x += inputX * p.swingPush * dt;
    } else if (swimming) {
        velocity.x = approach(velocity.x, inputX * p.swimSpeed, p.swimAcceleration * dt);
        // A stroke now and then, to see and hear.
        strokeTimer -= dt;
        if (inputX != 0.f && strokeTimer <= 0.f) {
            strokeTimer = 0.45f;
            events.push_back({PlayerEvent::Type::Stroked, position + sf::Vector2f(WIDTH * 0.5f, depth)});
        }
    } else {
        const float speed = std::abs(velocity.x);
        const bool along = inputX * velocity.x > 0.f; // holding the way you're going
        // On ice there's hardly any grip: slow to start, slow to stop.
        const bool ice = grounded && level->isOnIce(position, WIDTH);
        const float groundAcceleration = ice ? p.iceAcceleration : p.runAcceleration;
        const float groundDeceleration = ice ? p.iceDeceleration : p.runDeceleration;
        if (speed > p.runSpeed && (inputX == 0.f || along)) {
            // Faster than running, after a swing: slowing back down, holding on or not.
            const float slowing = grounded ? groundDeceleration : p.flingDeceleration;
            velocity.x = sign(velocity.x) * std::max(p.runSpeed, speed - slowing * dt);
        } else if (inputX != 0.f) {
            // Towards top speed. Turning round brakes as well, so it's quicker than starting.
            const float acceleration = grounded ? groundAcceleration : p.airAcceleration;
            const float braking = along || velocity.x == 0.f ? 0.f : grounded ? groundDeceleration : p.airDeceleration;
            velocity.x = approach(velocity.x, inputX * p.runSpeed, (acceleration + braking) * dt);
        } else {
            velocity.x = approach(velocity.x, 0.f, (grounded ? groundDeceleration : p.airDeceleration) * dt);
        }
    }
    // Wind pushes whatever's in the air: jumping, falling, swinging.
    if (!grounded && !clinging && !swimming && !riding) {
        velocity.x += level->wind() * dt;
    }

    // --- Vertical ---
    if (velocity.y <= 0.f) jumping = false;
    if (floating) {
        jumping = false; // pushed about above, nothing pulls
    } else if (swimming && !clinging && !rope) {
        // Floating: the water lifts you back up to the surface like a spring, and slows you, so you
        // bob there. Holding down dives; up swims back up.
        jumping = false;
        if (input.moveY < 0.f) {
            velocity.y = approach(velocity.y, -p.swimSpeed, p.swimAcceleration * dt);
        } else if (input.moveY > 0.f && depth > FLOAT_DEPTH + 2.f) {
            velocity.y = approach(velocity.y, p.swimSpeed, p.swimAcceleration * dt);
        } else if (velocity.y < -p.swimSpeed) {
            // Falling in: the water stops you soon after the splash.
            velocity.y = approach(velocity.y, -p.swimSpeed, 1500.f * dt);
        } else {
            velocity.y += std::clamp((depth - FLOAT_DEPTH) * p.buoyancy, -400.f, 400.f) * dt;
            velocity.y -= velocity.y * std::min(1.f, p.waterDrag * dt);
        }
    } else if (!clinging) {
        float gravity = p.gravity;
        if (jumping && !input.jumpHeld) gravity *= p.jumpReleaseGravity;
        velocity.y -= gravity * dt;
        if (velocity.y < -p.maxFallSpeed) velocity.y = -p.maxFallSpeed;
        // Heat rising: lifted, up to a point.
        if (level->updraftAt(exactCentre()) > 0.f && velocity.y < p.updraftSpeed) {
            velocity.y = std::min(velocity.y + p.updraftLift * dt, p.updraftSpeed);
            jumping = false;
        }
    }
    if (grounded && velocity.y < 0.f && !floating) {
        // Standing: don't let gravity pile up in the sub-pixel remainder.
        velocity.y = 0.f;
        remainder.y = 0.f;
    }
    if (grounded && !floating && !riding && !rope && input.moveY >= 0.f && velocity.y <= 0.f && level->isOnBounce(position, WIDTH)) {
        // On a bounce mushroom: thrown up, high (holding down, you stand on it).
        velocity.y = p.bounceSpeed;
        jumping = false;
        airJumped = false;
        coyoteTimer = 0.f;
        jumpBufferTimer = 0.f;
        squash = {0.7f, 1.35f};
        events.push_back({PlayerEvent::Type::Bounced, position + sf::Vector2f(WIDTH * 0.5f, 0.f)});
    }

    if (jumpBufferTimer > 0.f && floating) {
        jumpBufferTimer = 0.f; // nothing to jump off
    } else if (jumpBufferTimer > 0.f) {
        if (clinging) {
            // Leaping off the wall, away from it.
            velocity = {-static_cast<float>(clinging) * p.wallJumpPush, p.jumpSpeed};
            facingLeft = velocity.x < 0.f;
            clinging = 0;
            regrabTimer = REGRAB_TIME;
            stamina -= WALL_JUMP_STAMINA;
            jumping = true;
            jumpBufferTimer = 0.f;
            squash = {0.75f, 1.3f};
            events.push_back({PlayerEvent::Type::WallJumped, position + sf::Vector2f(WIDTH * 0.5f, HEIGHT * 0.5f)});
        } else if (swimming && !rope) {
            jumpBufferTimer = 0.f;
            if (depth <= FLOAT_DEPTH + 3.f) {
                // At the surface: a leap out of the water.
                velocity.y = p.swimJumpSpeed;
                jumping = true;
                squash = {0.8f, 1.25f};
                events.push_back({PlayerEvent::Type::Splashed, position + sf::Vector2f(WIDTH * 0.5f, depth), p.swimJumpSpeed});
            } else {
                // Under it: a kick up.
                velocity.y = std::max(velocity.y, p.swimSpeed * 1.5f);
                events.push_back({PlayerEvent::Type::Stroked, position + sf::Vector2f(WIDTH * 0.5f, HEIGHT)});
            }
        } else if (coyoteTimer > 0.f) {
            velocity.y = p.jumpSpeed;
            jumping = true;
            jumpBufferTimer = 0.f;
            coyoteTimer = 0.f;
            squash = {0.75f, 1.3f};
            events.push_back({PlayerEvent::Type::Jumped, position + sf::Vector2f(WIDTH * 0.5f, 0.f)});
        } else if (rope || (ropeJumpTimer > 0.f && p.ropeJumpSpeed > 0.f)) {
            // Jumping on the rope lets go of it. Where you fly is down to the swing and when you let
            // go, unless the physics give the jump a kick of its own.
            if (rope) letGoOfRope();
            jumpBufferTimer = 0.f;
            ropeJumpTimer = 0.f;
            if (p.ropeJumpSpeed > 0.f) {
                velocity.y = std::max(velocity.y, p.ropeJumpSpeed);
                jumping = true;
                squash = {0.75f, 1.3f};
            }
        } else if (canDoubleJump && !airJumped) {
            velocity.y = std::max(velocity.y, p.airJumpSpeed);
            jumping = true;
            jumpBufferTimer = 0.f;
            airJumped = true;
            squash = {0.8f, 1.25f};
            events.push_back({PlayerEvent::Type::AirJumped, position + sf::Vector2f(WIDTH * 0.5f, 0.f)});
        }
    }

    // --- Rope ---
    if (rope) {
        // On a ring, it goes where the ring goes. In rock that's crumbled away, it's let go.
        if (ringAnchor) rope->moveAnchor(level->ringPosition(*ringAnchor));
        else if (!anchorHeld()) letGoOfRope();
    }
    if (rope) {
        if (input.moveY != 0.f) rope->setLength(rope->length() - input.moveY * p.reelSpeed * dt);

        float speed = magnitude(velocity);
        if (swinging && speed > p.maxSwingSpeed) velocity *= p.maxSwingSpeed / speed;
        velocity = rope->constrainVelocity(exactCentre(), velocity);
    }

    const sf::Vector2f before = exactCentre();
    const float fallSpeed = -velocity.y;
    moveX(velocity.x * dt);
    moveY(velocity.y * dt);

    if (!grounded && isGrounded() && fallSpeed > 60.f) {
        float amount = std::min(1.f, fallSpeed / p.maxFallSpeed);
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
    if (loose) loose->update(exactCentre(), dt, hook == Hook::Out);
}

void Player::updateThrow(float dt, bool grounded)
{
    const Physics& p = physics();
    // Floating, there's nothing to stand on, and nothing to fall off either.
    const bool footing = grounded || clinging != 0 || isFloating();
    switch (hook) {
        case Hook::InHand:
            if (ropeShotQueued && !rope && hasRope && !riding && (footing || throwsInAir)) {
                ropeShotQueued = false;
                // Not yet sure-handed: swung round a few times first, standing.
                if (!throwsInAir && p.throwWindup > 0.f) {
                    hook = Hook::WindingUp;
                    windUp = p.throwWindup;
                } else {
                    throwRope();
                }
            }
            break;
        case Hook::WindingUp:
            windUp -= dt;
            if (!footing) hook = Hook::InHand; // jumped or fell off: no throw
            else if (windUp <= 0.f) throwRope();
            break;
        case Hook::Out: {
            // After a ring, wherever it's got to.
            if (hookKind == RopeHit::Kind::Ring) hookTarget = level->ringPosition(static_cast<size_t>(hookIndex));
            const sf::Vector2f to = hookTarget - hookHead;
            const float left = magnitude(to), step = p.throwSpeed * dt;
            if (left > step) {
                hookHead += to / left * step;
                loose->flyTo(hookHead);
                break;
            }
            hookHead = hookTarget;
            loose->flyTo(hookHead);
            // It holds if the rope still reaches: you may have moved while it flew.
            if (hookCatches && distance(exactCentre(), hookTarget) <= ropeRange + 4.f) {
                rope.emplace(exactCentre(), hookTarget, level);
                if (hookKind == RopeHit::Kind::Ring) ringAnchor = static_cast<size_t>(hookIndex);
                loose.reset();
                hook = Hook::InHand;
                airJumped = false;
                clinging = 0;
                events.push_back({PlayerEvent::Type::RopeAttached, hookTarget});
            } else if (hookKind == RopeHit::Kind::Lever || hookKind == RopeHit::Kind::Lamp) {
                // A tug, and back it comes.
                const auto type = hookKind == RopeHit::Kind::Lever ? PlayerEvent::Type::LeverTugged : PlayerEvent::Type::LampTugged;
                events.push_back({type, hookTarget, 0.f, hookIndex});
                hook = Hook::Back;
            } else {
                if (hookBounces) {
                    loose->bounce(exactCentre() - hookTarget);
                    events.push_back({PlayerEvent::Type::RopeBounced, hookTarget});
                }
                hook = Hook::Back;
            }
            break;
        }
        case Hook::Back:
            // Pulled in hand over hand, the way it lies.
            loose->reelIn(p.ropeReturnSpeed * dt);
            if (loose->isIn()) {
                loose.reset();
                hook = Hook::InHand;
            }
            break;
    }
}

void Player::throwRope()
{
    const sf::Vector2f from = exactCentre();
    sf::Vector2f direction = input.aim - from;
    const float length = magnitude(direction);
    direction = length > 0.f ? direction / length : sf::Vector2f(facingLeft ? -1.f : 1.f, 0.f);
    hookHead = from;
    loose.emplace(from, level);
    hookCatches = hookBounces = false;
    hookKind = RopeHit::Kind::Rock;
    hookIndex = -1;
    if (const auto hit = level->ropeHit(from, from + direction, reach())) {
        hookTarget = hit->point;
        hookKind = hit->kind;
        hookIndex = hit->index;
        if (hit->kind == RopeHit::Kind::Ring) {
            hookCatches = true;
        } else if (hit->kind == RopeHit::Kind::Rock && hit->distance >= Rope::MIN_LENGTH) {
            hookCatches = true;
        } else if (hit->kind == RopeHit::Kind::Rock || hit->kind == RopeHit::Kind::Slick) {
            // Slick stone, or rock too close to hook on: it bounces off it, not through it.
            hookBounces = true;
        }
    } else {
        hookTarget = from + direction * reach();
    }
    hook = Hook::Out;
    events.push_back({PlayerEvent::Type::RopeThrown, from});
}

void Player::letGoOfRope()
{
    loose.emplace(rope->path(), level);
    hook = Hook::Back;
    rope.reset();
    ringAnchor.reset();
}

bool Player::anchorHeld() const
{
    // The rock the anchor's in: right next to it.
    const sf::Vector2f anchor = rope->anchor();
    for (float dx : {-0.5f, 0.5f}) {
        for (float dy : {-0.5f, 0.5f}) {
            const auto [x, y] = tailNumbers(anchor + sf::Vector2f(dx, dy));
            if (level->isSolid(x, y)) return true;
        }
    }
    return false;
}

bool Player::wallToGrab(int side) const
{
    // Rock (or a gate, or a hook block) right beside your hands. Not slick stone.
    const float x = side > 0 ? position.x + WIDTH + 0.5f : position.x - 0.5f;
    const auto [tileX, tileY] = tailNumbers({x, position.y + HEIGHT - 2.f});
    return level->isSolid(tileX, tileY) && !level->isSlick(tileX, tileY) && !level->isIce(tileX, tileY);
}

void Player::updateClimbing(float dt, bool grounded)
{
    const Physics& p = physics();
    if (!canClimb || rope || riding || isFloating()) {
        clinging = 0;
        return;
    }
    const int toward = input.moveX > 0.f ? 1 : input.moveX < 0.f ? -1 : 0;
    if (clinging) {
        if (toward != clinging || (grounded && input.moveY <= 0.f)) {
            clinging = 0; // let go, or standing at the foot of it
            return;
        }
        if (!wallToGrab(clinging)) {
            // Hands over the top while climbing: a hop up onto it. Otherwise off the end of it.
            const float x = clinging > 0 ? position.x + WIDTH + 0.5f : position.x - 0.5f;
            const auto [tileX, tileY] = tailNumbers({x, position.y + 1.f});
            if (input.moveY > 0.f && level->isSolid(tileX, tileY)) {
                velocity = {static_cast<float>(clinging) * 30.f, p.climbOverSpeed};
            }
            clinging = 0;
            return;
        }
    } else {
        if (toward == 0 || regrabTimer > 0.f || stamina <= 0.f || !wallToGrab(toward)) return;
        // From the ground only by climbing up; in the air once a jump has slowed down.
        if (grounded ? input.moveY <= 0.f : velocity.y > GRAB_BELOW_SPEED) return;
        clinging = toward;
        events.push_back({PlayerEvent::Type::Grabbed, position + sf::Vector2f(clinging > 0 ? WIDTH : 0.f, HEIGHT - 2.f)});
    }

    facingLeft = clinging < 0;
    velocity.x = 0.f;
    remainder.x = 0.f;
    jumping = false;
    if (stamina <= 0.f) {
        velocity.y = -p.wallSlideSpeed; // too tired to hold on: sliding down
    } else if (input.moveY > 0.f) {
        velocity.y = p.climbSpeed;
        stamina -= 2.f * dt;
    } else {
        velocity.y = input.moveY < 0.f ? -p.climbDownSpeed : 0.f;
        stamina -= dt;
    }
}

void Player::moveX(float amount)
{
    remainder.x += amount;
    int move = static_cast<int>(std::round(remainder.x));
    if (move == 0) return;
    remainder.x -= static_cast<float>(move);

    const int step = move > 0 ? 1 : -1;
    // Swimming, a ledge at the water's edge is a step up out of it.
    const bool airborne = !isGrounded() || swimming;
    const int popHeight = swimming ? WATER_STEP : CORNER_CORRECTION;
    while (move != 0) {
        sf::Vector2f next = position + sf::Vector2f(static_cast<float>(step), 0.f);
        if (!fits(next)) {
            // In the air, a ledge lip just below your feet pops you up onto it instead of stopping you.
            bool popped = false;
            for (int up = 1; airborne && up <= popHeight && !popped; ++up) {
                sf::Vector2f lifted(0.f, static_cast<float>(up));
                if (fits(position + lifted) && fits(next + lifted)) {
                    next += lifted;
                    popped = true;
                }
            }
            if (!popped) {
                velocity.x = 0.f;
                remainder.x = 0.f;
                if (riding) crashed = true; // a cart doesn't stop for walls
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
    // The rope flying out or coming back: a straight line to the hook, from your hand.
    const auto dot = [&](sf::Vector2f world, sf::Color colour) {
        const sf::Vector2f at = viewCoordinates(world, centr);
        sf::RectangleShape shape({2.f, 2.f});
        shape.setPosition(std::floor(at.x) - 1.f, std::floor(at.y) - 1.f);
        shape.setFillColor(colour);
        target.draw(shape);
    };
    const sf::Vector2f hand = centre() + sf::Vector2f(0.f, 2.f);
    if (loose) {
        loose->draw(target, centr);
    } else if (hook == Hook::WindingUp) {
        // Swung round over your head.
        const float angle = windUp * 30.f;
        const sf::Vector2f over = hand + sf::Vector2f(0.f, 7.f), head = over + sf::Vector2f(std::cos(angle), std::sin(angle) * 0.5f) * 6.f;
        const sf::Vector2f a = viewCoordinates(hand, centr), b = viewCoordinates(head, centr);
        const sf::Vertex line[] = {{{std::floor(a.x) + 0.5f, std::floor(a.y) + 0.5f}, ROPE_COLOUR},
                                   {{std::floor(b.x) + 0.5f, std::floor(b.y) + 0.5f}, ROPE_COLOUR}};
        target.draw(line, 2, sf::Lines);
        dot(head, HOOK_COLOUR);
    }

    int frame;
    bool lyingInWater = false;
    if (sitting && isGrounded()) {
        frame = frames::SIT;
    } else if (sitting && swimming) {
        // Coming to, floating on your back.
        frame = frames::IDLE;
        lyingInWater = true;
    } else if (riding) {
        frame = frames::IDLE; // standing in the cart, holding on
    } else if (isFloating() && !rope) {
        frame = frames::FALL; // hair and arms adrift
    } else if (swimming && !rope) {
        frame = std::abs(velocity.x) > 5.f ? frames::RUN + static_cast<int>(animTime * 7.f) % 4 : frames::IDLE + static_cast<int>(animTime * 2.f) % 2;
    } else if (clinging) {
        frame = frames::CLIMB + (velocity.y != 0.f ? static_cast<int>(animTime * 6.f) % 2 : 0);
    } else if (hook == Hook::WindingUp) {
        frame = frames::SWING;
    } else if (!isGrounded()) {
        frame = rope ? frames::SWING : velocity.y > 0.f ? frames::JUMP : frames::FALL;
    } else if (std::abs(velocity.x) > 5.f) {
        frame = frames::RUN + static_cast<int>(animTime * 12.f) % 4;
    } else {
        frame = frames::IDLE + static_cast<int>(animTime * 2.f) % 2;
    }

    sf::Sprite sprite(texture);
    sprite.setTextureRect(sf::IntRect(frame * frames::WIDTH, static_cast<int>(look) * frames::HEIGHT, frames::WIDTH, frames::HEIGHT));
    // Anchored at the middle of the feet, so squashing and flipping keep the player planted.
    // The frames have a 1px outline below the feet, so they sit one pixel lower.
    sprite.setOrigin(frames::WIDTH * 0.5f, frames::HEIGHT - 1.f);
    sprite.setScale(facingLeft ? -squash.x : squash.x, squash.y);
    // In a cart, up on its floor, its sides in front of your legs.
    sprite.setPosition(viewCoordinates(position + sf::Vector2f(WIDTH * 0.5f, riding ? 3.f : 0.f), centr));
    if (lyingInWater) {
        // Head to the left, face up, the middle of you at the surface.
        sprite.setOrigin(frames::WIDTH * 0.5f, frames::HEIGHT * 0.5f);
        sprite.setScale(1.f, 1.f);
        sprite.setRotation(-90.f);
        const sf::Vector2f at = viewCoordinates(position + sf::Vector2f(WIDTH * 0.5f, FLOAT_DEPTH + 1.f), centr);
        sprite.setPosition(std::floor(at.x), std::floor(at.y));
    }
    target.draw(sprite);
}

void Player::drawFigure(sf::RenderTarget& target, size_t lookIndex, sf::Vector2f feet, bool facingLeft, float time, float scale,
                        bool sittingDown) const
{
    drawPose(target, lookIndex, sittingDown ? Pose::Sit : Pose::Idle, feet, facingLeft, time, scale);
}

void Player::drawPose(sf::RenderTarget& target, size_t lookIndex, Pose pose, sf::Vector2f feet, bool facingLeft, float time, float scale) const
{
    int frame = frames::IDLE;
    switch (pose) {
        case Pose::Idle: frame = frames::IDLE + static_cast<int>(time * 2.f) % 2; break;
        case Pose::Run: frame = frames::RUN + static_cast<int>(time * 12.f) % 4; break;
        case Pose::Jump: frame = frames::JUMP; break;
        case Pose::Fall: frame = frames::FALL; break;
        case Pose::Swing: frame = frames::SWING; break;
        case Pose::Sit: frame = frames::SIT; break;
        case Pose::Climb: frame = frames::CLIMB + static_cast<int>(time * 6.f) % 2; break;
    }
    sf::Sprite sprite(texture);
    sprite.setTextureRect(sf::IntRect(frame * frames::WIDTH, static_cast<int>(std::min(lookIndex, LOOKS.size() - 1)) * frames::HEIGHT,
                                      frames::WIDTH, frames::HEIGHT));
    sprite.setOrigin(frames::WIDTH * 0.5f, frames::HEIGHT - 1.f);
    sprite.setScale(facingLeft ? -scale : scale, scale);
    sprite.setPosition(feet);
    target.draw(sprite);
}

size_t Player::lookNamed(const std::string& name)
{
    for (size_t ii = 0; ii < LOOKS.size(); ++ii) {
        if (name == LOOKS[ii]) return ii;
    }
    return 0;
}

void Player::setSitting(bool sit)
{
    sitting = sit;
}

void Player::setLook(size_t lookIndex)
{
    look = std::min(lookIndex, LOOKS.size() - 1);
}

void Player::enterLevel(std::shared_ptr<const Level> lvl, sf::Vector2f shift, bool fromBelow)
{
    level = std::move(lvl);
    position += shift;
    rope.reset();
    ringAnchor.reset();
    loose.reset();
    hook = Hook::InHand;
    clinging = 0;
    ropeJumpTimer = 0.f;
    if (fromBelow) {
        velocity.y = std::max(velocity.y, physics().jumpSpeed);
        jumping = false; // a full lift even if jump was let go of
    }
}

void Player::respawn(sf::Vector2f pos)
{
    position = {std::round(pos.x), std::round(pos.y)};
    remainder = {0.f, 0.f};
    velocity = {0.f, 0.f};
    rope.reset();
    ringAnchor.reset();
    loose.reset();
    hook = Hook::InHand;
    clinging = 0;
    riding = 0;
    crashed = false;
    stamina = physics().climbStamina;
    regrabTimer = 0.f;
    coyoteTimer = 0.f;
    jumpBufferTimer = 0.f;
    dropThroughTimer = 0.f;
    ropeJumpTimer = 0.f;
    ropeShotQueued = false;
    airJumped = false;
    squash = {1.f, 1.f};
    jumping = false;
    swimming = isSwimming();
    strokeTimer = 0.f;
    underwater = false;
    air = maxAir();
    bubbleTimer = 0.3f;
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

sf::FloatRect Player::hitbox() const { return {position.x, position.y, WIDTH, HEIGHT}; }

sf::Vector2f Player::getVelocity() const { return velocity; }

void Player::setVelocity(sf::Vector2f v) { velocity = v; }

bool Player::isGrounded() const
{
    if (!level->isEmptySpace(position + sf::Vector2f(0.f, -1.f), 1.f, WIDTH)) return true;
    return dropThroughTimer <= 0.f && level->isOnPlatform(position, WIDTH);
}

bool Player::isSwimming() const
{
    const auto depth = level->depthInWater({position.x + WIDTH * 0.5f, position.y + 0.5f});
    return depth && *depth + 0.5f >= SWIM_DEPTH;
}

bool Player::isUnderwater() const
{
    return level->depthInWater(position + sf::Vector2f(WIDTH * 0.5f, MOUTH_HEIGHT)).has_value();
}

float Player::maxAir() const
{
    return physics().airTime * breathScale;
}

float Player::airLeft() const
{
    return maxAir() > 0.f ? std::clamp(air / maxAir(), 0.f, 1.f) : 1.f;
}

void Player::setAirLeft(float left)
{
    air = std::clamp(left, 0.f, 1.f) * maxAir();
}

void Player::setBreathScale(float scale)
{
    const float left = airLeft();
    breathScale = scale;
    air = left * maxAir();
}

bool Player::isFloating() const
{
    return level->isSpace();
}

void Player::startRiding(int dir)
{
    riding = dir < 0 ? -1 : 1;
    crashed = false;
    clinging = 0;
    facingLeft = riding < 0;
    if (rope) letGoOfRope();
}

void Player::stopRiding()
{
    riding = 0;
    velocity.x = 0.f;
}

bool Player::isRiding() const
{
    return riding != 0;
}

int Player::ridingDir() const
{
    return riding;
}

bool Player::hasCrashed() const
{
    return crashed;
}

bool Player::isOutOfAir() const
{
    return air <= 0.f;
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

std::optional<RopeHit> Player::aimHit(sf::Vector2f aim) const
{
    return level->ropeHit(exactCentre(), aim, reach());
}

std::optional<sf::Vector2f> Player::ropeTarget(sf::Vector2f aim) const
{
    const auto hit = aimHit(aim);
    if (!hit) return std::nullopt;
    if (hit->kind == RopeHit::Kind::Ring || (hit->kind == RopeHit::Kind::Rock && hit->distance >= Rope::MIN_LENGTH)) return hit->point;
    return std::nullopt;
}

sf::Vector2f Player::aimAlong(sf::Vector2f direction) const
{
    const sf::Vector2f from = exactCentre();
    const float straight = std::atan2(direction.y, direction.x);
    const auto along = [&](float angle) { return from + sf::Vector2f(std::cos(angle), std::sin(angle)) * reach(); };
    // Out from the straight line both ways at once, so the nearest wins. Not the ground under you,
    // unless it's pointing down.
    constexpr int STEPS = 16;
    for (int step = 0; step <= STEPS; ++step) {
        for (float side : {1.f, -1.f}) {
            const sf::Vector2f aim = along(straight + side * AIM_ASSIST * float(step) / STEPS);
            const auto hit = ropeTarget(aim);
            if (hit && (direction.y < 0.f || hit->y >= from.y)) return aim;
        }
    }
    return along(straight);
}

bool Player::isFacingLeft() const
{
    return facingLeft;
}

std::optional<sf::Vector2f> Player::ropeBounce(sf::Vector2f aim) const
{
    const auto hit = aimHit(aim);
    if (!hit) return std::nullopt;
    if (hit->kind == RopeHit::Kind::Slick || (hit->kind == RopeHit::Kind::Rock && hit->distance < Rope::MIN_LENGTH)) return hit->point;
    return std::nullopt;
}

float Player::reach() const
{
    return isGrounded() || clinging || isFloating() ? ropeRange : ropeRange * physics().airThrowRange;
}

void Player::setHasRope(bool has)
{
    hasRope = has;
    if (!has) {
        rope.reset();
        ringAnchor.reset();
        loose.reset();
        hook = Hook::InHand;
    }
}

void Player::setCanThrowInAir(bool can)
{
    throwsInAir = can;
}

void Player::setCanClimb(bool can)
{
    canClimb = can;
    if (!can) clinging = 0;
}

void Player::setRopeRange(float range)
{
    ropeRange = range;
}

void Player::setCanDoubleJump(bool can)
{
    canDoubleJump = can;
}

bool Player::hasAirJump() const
{
    return canDoubleJump && !airJumped;
}


bool Player::canShootRope() const
{
    return hasRope && hook == Hook::InHand && !rope && !riding && (throwsInAir || isGrounded() || clinging || isFloating());
}

bool Player::isClinging() const
{
    return clinging != 0;
}

float Player::staminaLeft() const
{
    return physics().climbStamina > 0.f ? std::clamp(stamina / physics().climbStamina, 0.f, 1.f) : 0.f;
}
