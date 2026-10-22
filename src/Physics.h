#pragma once

#include <string>
#include <vector>

// Every number that decides how the player moves, in one place. The game reads them from
// assets/physics.txt when it starts, and again on F5 while playing, so the feel can be tuned by
// playing it: change a number, press F5, try it. Distances are in pixels (8 to a tile), times in
// seconds. The defaults here are what the tests expect; physics.txt can say otherwise.
struct Physics
{
    // Running
    float runSpeed = 70.f;           // top speed on the ground, px/s
    float runAcceleration = 300.f;   // px/s each second: 70 / 300 is a quarter of a second to top speed
    float runDeceleration = 450.f;   // slowing down with nothing held (turning round brakes and accelerates)
    float airAcceleration = 220.f;   // the same in the air, where there's less grip
    float airDeceleration = 60.f;    // nothing held in the air: you mostly drift on
    float flingDeceleration = 140.f; // in the air faster than running, e.g. after letting go of a swing

    // Jumping and falling
    float gravity = 800.f;
    float jumpSpeed = 215.f;         // straight up, from the ground
    float jumpReleaseGravity = 3.f;  // gravity times this after letting go of jump early: short hops
    float maxFallSpeed = 300.f;
    float coyoteTime = 0.1f;         // a jump still works this long after running off a ledge
    float jumpBufferTime = 0.1f;     // and pressed this long before landing
    float airJumpSpeed = 195.f;      // the double jump

    // The rope. It's thrown, flies out, and either catches or comes back; after letting go of it, it
    // comes back too. It can only be thrown again once it's back in your hand.
    float ropeRange = 64.f;          // how far a throw from the ground reaches, from your middle
    float longRopeRange = 104.f;     // with the long rope
    float airThrowRange = 0.85f;     // a throw in the air reaches this much as far: nothing to push off
    float throwWindup = 0.3f;        // swinging it round before a throw, until you can throw in the air
    float throwSpeed = 320.f;        // how fast it flies out
    float ropeReturnSpeed = 120.f;   // and comes back
    float swingPush = 140.f;         // holding left or right on the rope, below where it hangs from
    float maxSwingSpeed = 210.f;
    float reelSpeed = 60.f;          // up and down on the rope
    float ropeJumpSpeed = 0.f;       // jumping on the rope lets go, with this much upward speed. 0: just lets go

    // Climbing walls with your hands (the chalk bag)
    float climbSpeed = 40.f;         // up
    float climbDownSpeed = 70.f;
    float climbStamina = 4.f;        // seconds of holding on (climbing up uses it twice as fast), back on the ground
    float wallSlideSpeed = 40.f;     // sliding down, out of stamina
    float wallJumpPush = 110.f;      // jumping off a wall: away from it this fast, and up as a jump
    float climbOverSpeed = 150.f;    // climbing over the top: a hop up onto it

    // "run_speed 70" lines; # starts a comment. Unknown names and broken numbers are skipped and
    // described in `problems`, if given.
    static Physics parse(const std::string& text, std::vector<std::string>* problems = nullptr);
    std::string serialize() const; // every number, by name
};

// The numbers in use.
const Physics& physics();
void setPhysics(const Physics& numbers);
// Reads a physics file into physics(). What was wrong with it, if anything: a missing file is the
// defaults and one problem.
std::vector<std::string> loadPhysics(const std::string& path);
