#pragma once

#include "src/Player.h"
#include <SFML/Graphics.hpp>
#include <deque>
#include <optional>

// Pip, your dog, once you've found him: he goes where you went, a little way behind, the same way
// you did (jumping where you jumped, swimming where you swam), and sits down when you stop. He can
// smell gems: now and then he points his nose at the nearest one still to find, and barks.
class Dog
{
public:
    // How far behind you he keeps, along the way you went, in pixels.
    static constexpr float BEHIND = 14.f;

    // Standing at `feet` (the middle of the bottom), anything behind forgotten: after travelling,
    // or coming back from dying.
    void reset(sf::Vector2f feet);
    // Moving into another room: everything by `shift`, into its coordinates.
    void shift(sf::Vector2f by);
    // Once a step: where your feet are now.
    void follow(sf::Vector2f feet, float dt);
    // Nose pointed at that, and barking, for a moment.
    void point(sf::Vector2f at);
    bool isPointing() const;

    sf::Vector2f feet() const;
    void draw(sf::RenderTarget& target, const Player& looks, sf::Vector2f centr, float time) const;

private:
    std::deque<sf::Vector2f> path; // the way you went, oldest first: he's at the front
    sf::Vector2f at;               // his feet
    sf::Vector2f moved;            // since the last step
    bool facingLeft = false;
    float still = 0.f;             // seconds without moving
    float pointing = 0.f;          // seconds left of it
};
