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
    float airAcceleration = 100.f;   // the same in the air: a little steering, you mostly go where the jump sent you
    float airDeceleration = 0.f;     // nothing held doesn't slow you down
    float flingDeceleration = 140.f; // in the air faster than running, e.g. after letting go of a swing

    // Jumping and falling
    float gravity = 800.f;
    float jumpSpeed = 215.f;         // straight up, from the ground
    float jumpReleaseGravity = 3.f;  // gravity times this after letting go of jump early: short hops
    float maxFallSpeed = 300.f;
    float iceAcceleration = 100.f;   // on ice: slow to get going...
    float iceDeceleration = 40.f;    // ...and slower to stop
    float updraftLift = 1400.f;      // an updraft pushing up, against gravity
    float updraftSpeed = 180.f;      // up to this fast
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

    // Swimming. In water you float with your head and shoulders out, bobbing.
    float swimSpeed = 45.f;          // across, and down holding down
    float swimAcceleration = 220.f;
    float buoyancy = 70.f;           // how hard the water lifts you back to floating, px/s each second for each px too deep
    float waterDrag = 5.f;           // how quickly the water slows you up and down: this much of the speed a second
    float swimJumpSpeed = 200.f;     // jumping out from the surface: enough to get onto a ledge at the water's edge
    float airTime = 8.f;             // seconds of breath under water, before you drown
    float airRefillTime = 1.f;       // seconds at the surface to get it all back
    float divingAir = 4.f;           // the diving suit makes your breath last this many times as long

    // Bounce mushrooms: landing on one throws you up this fast (holding down, you stand on it).
    float bounceSpeed = 330.f;

    // The rope tied off (with the knot): a line to hang from.
    float lineHandSpeed = 45.f;      // along it hand over hand, where it isn't steep
    float lineSteep = 0.45f;         // steeper than this (how much of it is up and down), you slide down it...
    float lineZipSpeed = 200.f;      // ...up to this fast
    float lineClimbSpeed = 30.f;     // or climb up it this fast
    float lineJumpSpeed = 230.f;     // jumping off it: enough to get onto a ledge from a line hooked at its lip

    // Stone blocks: hooked, and reeled in with your feet braced, the rope drags one this fast.
    float stoneDragSpeed = 40.f;

    // Riding a mine cart along the rails: it rolls on at this speed, and jump jumps it.
    float cartSpeed = 110.f;

    // Floating where there's no gravity (the Still). Out in the open nothing you do moves you: you
    // drift on the way you were going. Only the rope gets you anywhere.
    float floatDrag = 0.1f;          // how much of your speed you lose each second, drifting
    float floatCrawl = 30.f;         // holding on to rock, the arrows move you along it (never off it), this fast
    float throwRecoil = 16.f;        // throwing the rope pushes you the other way, this fast
    float spaceReelPull = 320.f;     // reeling in on the rope pulls you along it, px/s each second...
    float spaceReelSpeed = 130.f;    // ...up to this fast

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
