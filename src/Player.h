#pragma once

#include "src/Level.h"
#include "src/Rope.h"
#include "src/ThrownRope.h"
#include <SFML/Graphics.hpp>
#include <array>
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
    enum class Type { Jumped, AirJumped, WallJumped, Landed, Stepped, Grabbed, RopeThrown, RopeAttached, RopeWrapped, RopeBounced,
                      LeverTugged, LampTugged };
    Type type;
    sf::Vector2f position;
    float strength = 0.f; // Landed: how fast the player was falling
    int index = -1;       // LeverTugged, LampTugged: which, in the level's list
};

class Player
{
public:
    explicit Player(std::shared_ptr<const Level> lvl);

    // The looks to choose from, in the order of the rows in assets/gfx/player.png (made by
    // tools/make_art.py). Only the sprite changes: every skin has the same hitbox.
    static constexpr std::array<const char*, 15> SKINS = {
        "classic", "kid", "climber", "explorer", "astronaut", "frog", "knight", "fox", "robot", "mushroom",
        "scholar", "gardener", "fisher", "smith", "scarf"};
    // Every look in player.png: the skins, then the people you meet, who aren't for choosing.
    static constexpr std::array<const char*, 24> LOOKS = {
        "classic", "kid", "climber", "explorer", "astronaut", "frog", "knight", "fox", "robot", "mushroom",
        "scholar", "gardener", "fisher", "smith", "scarf",
        "wren", "pip", "moss", "juniper", "ash", "tuck", "bramble", "pell", "cinder"};
    void setSkin(size_t index);
    // The skin running on the spot, standing at `feet`, for the options screen.
    void drawPreview(sf::RenderTarget& target, sf::Vector2f feet, float scale, float time) const;
    // Someone else standing still in one of the LOOKS, breathing, with their feet at `feet`
    // (screen pixels): the characters in the world and their faces in conversations.
    void drawFigure(sf::RenderTarget& target, size_t lookIndex, sf::Vector2f feet, bool facingLeft, float time, float scale = 1.f) const;
    // The index of a look in LOOKS by its name, or 0 if there's none called that.
    static size_t lookNamed(const std::string& name);
    // Sitting on a bench: drawn sitting, until moving again.
    void setSitting(bool sit);

    // Once per rendered frame: takes the buttons held and pressed (a rope press waits to be thrown,
    // a jump press is buffered).
    void handleInput(const PlayerInput& input);
    // Once per fixed physics step.
    void update(float dt);
    void draw(sf::RenderTarget& target, sf::Vector2f centr) const;

    void respawn(sf::Vector2f pos);
    // Carries on into the next room, moved by `shift` into its coordinates, keeping the speed but
    // letting go of the rope. Going up into a room through its floor gives a lift, like in
    // Celeste, so a jump through the gap makes it onto the ledge above.
    void enterLevel(std::shared_ptr<const Level> lvl, sf::Vector2f shift, bool fromBelow);

    sf::Vector2f centre() const;
    // Bottom-left corner of the hitbox. Always whole pixels.
    sf::Vector2f getPosition() const;
    sf::FloatRect hitbox() const;
    sf::Vector2f getVelocity() const;
    void setVelocity(sf::Vector2f v);
    bool isGrounded() const;
    bool isTouchingDanger() const;
    const Rope* getRope() const;
    // What a rope thrown towards `aim` now would meet first, within reach() (see Level::ropeHit).
    std::optional<RopeHit> aimHit(sf::Vector2f aim) const;
    // Where it would catch, if anywhere: on rock, a hook block or a ring, not on slick stone.
    std::optional<sf::Vector2f> ropeTarget(sf::Vector2f aim) const;
    // How far a throw now would reach: less in the air.
    float reach() const;
    // Where to aim to shoot the rope in `direction` (y up), with a gamepad's stick: at whatever
    // it would stick to nearest that direction, within AIM_ASSIST either side, since a stick can't
    // point as exactly as a mouse. If there's nothing, straight that way.
    sf::Vector2f aimAlong(sf::Vector2f direction) const;
    bool isFacingLeft() const;
    // The abilities. Without the rope there's nothing to throw. Until it can be thrown in the air,
    // it's only thrown standing (or holding on to a wall), and swung round first. With the chalk
    // bag, walls can be climbed.
    void setHasRope(bool has);
    void setCanThrowInAir(bool can);
    void setCanClimb(bool can);
    // How far the rope reaches: physics().ropeRange, or longRopeRange with the long rope.
    void setRopeRange(float range);
    // With the updraft feather: one more jump in the air, until landing or catching the rope.
    void setCanDoubleJump(bool can);
    bool hasAirJump() const; // it could double jump now
    // Where a rope thrown towards `aim` would hit something and bounce off: slick stone, or rock too
    // close to catch on.
    std::optional<sf::Vector2f> ropeBounce(sf::Vector2f aim) const;
    // The rope's in your hand and you're where it can be thrown from.
    bool canShootRope() const;
    // Holding on to a wall, and how much strength is left for it (0 to 1).
    bool isClinging() const;
    float staminaLeft() const;
    // Events since the last call.
    std::vector<PlayerEvent> takeEvents();

    // Dimensions
    static constexpr float WIDTH = 7.f;
    static constexpr float HEIGHT = 11.f;

    // How it moves is in physics() (src/Physics.h), read from assets/physics.txt.

    // The stick's aim reaches this far either side of where it points, in radians (about 23 degrees).
    static constexpr float AIM_ASSIST = 0.4f;
    // A jump just after letting go of the rope still counts as jumping off it.
    static constexpr float ROPE_JUMP_GRACE = 0.1f;
    // After leaping off a wall, it can't be grabbed again for this long.
    static constexpr float REGRAB_TIME = 0.2f;

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
    // The rope, when it isn't hooked.
    void updateThrow(float dt, bool grounded);
    void throwRope();
    void letGoOfRope(); // it falls from where it was hooked, and is reeled back in
    bool anchorHeld() const; // there's still rock where it's hooked (crumbling blocks give way)
    // Climbing: what's to grab on `side` (-1 left, 1 right) at hand height, and holding on.
    bool wallToGrab(int side) const;
    void updateClimbing(float dt, bool grounded);

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
    bool ropeShotQueued = false; // pressed: thrown as soon as it can be, if still held

    // The rope when it isn't hooked: in your hand, being swung round to throw, flying out to where
    // it'll catch or bounce off, or being reeled back in. Out and Back, `loose` is where it lies.
    enum class Hook { InHand, WindingUp, Out, Back };
    Hook hook = Hook::InHand;
    std::optional<ThrownRope> loose;
    float windUp = 0.f;           // time left swinging it round
    sf::Vector2f hookHead;        // where the hook is, flying Out
    sf::Vector2f hookTarget;      // where it's flying to
    bool hookCatches = false;     // and whether it'll stick there
    bool hookBounces = false;     // or hit something it won't stick to
    RopeHit::Kind hookKind = RopeHit::Kind::Rock;
    int hookIndex = -1;           // the ring, lever or lamp it's flying to
    std::optional<size_t> ringAnchor; // hooked on a moving ring
    bool hasRope = true;
    bool throwsInAir = true;

    // Climbing
    bool canClimb = false;
    int clinging = 0;             // the side of the wall held on to: -1 left, 1 right, 0 none
    float stamina = 0.f;
    float regrabTimer = 0.f;
    bool jumping = false;
    bool facingLeft = false;
    float ropeRange = 100.f;
    bool canDoubleJump = false;
    bool sitting = false;
    bool airJumped = false; // used the double jump since last on the ground or the rope

    // Looks
    size_t skin = 0;
    std::vector<PlayerEvent> events;
    float animTime = 0.f;
    float stepTimer = 0.f;
    sf::Vector2f squash{1.f, 1.f}; // sprite scale, eased back to 1 after jumps and landings
};
